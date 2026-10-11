#include <atomic>
#include "network_worker.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#if __has_include("device_secrets.h")
#include "device_secrets.h"
#endif
namespace {
QueueHandle_t requests=nullptr, results=nullptr;
bool started=false;
std::atomic<unsigned> pending{0};
void (*wakeHook)()=nullptr;
class AudioResponse : public Stream {
public:
  uint8_t* data=static_cast<uint8_t*>(ps_malloc(1280000)); size_t size=0; bool overflow=false;
  ~AudioResponse(){free(data);}
  int available() override{return 0;} int read() override{return -1;} int peek() override{return -1;} void flush() override{}
  size_t write(uint8_t b) override{return write(&b,1);}
  size_t write(const uint8_t* p,size_t n) override{
    if(!data || size+n>1280000)overflow=true;
    if(!overflow){memcpy(data+size,p,n);size+=n;} return n;
  }
};
class BoundedResponse : public Stream {
public:
  String body; bool overflow=false;
  int available() override{return 0;}
  int read() override{return -1;}
  int peek() override{return -1;}
  void flush() override{}
  size_t write(uint8_t byte) override{return write(&byte,1);}
  size_t write(const uint8_t* bytes,size_t count) override {
    if(!overflow) {
      if(body.length()+count>65536 || !body.concat(reinterpret_cast<const char*>(bytes),count))overflow=true;
    }
    return count; // Drain the response even when too large; never parse partial JSON.
  }
};
void worker(void*) {
  for(;;) {
    NetworkRequest* req=nullptr;
    if(xQueueReceive(requests,&req,portMAX_DELAY)!=pdTRUE)continue;
    uint32_t waiting=millis();while(WiFi.status()!=WL_CONNECTED && millis()-waiting<15000) vTaskDelay(pdMS_TO_TICKS(100));
    auto result=new NetworkResult; result->kind=req->kind;result->generation=req->generation;
#if defined(SUPABASE_URL) && defined(SUPABASE_PUBLISHABLE_KEY) && defined(SUPABASE_ROOT_CA)
    if(apiConfigured() && WiFi.status()==WL_CONNECTED) {
      WiFiClientSecure tls;tls.setCACert(SUPABASE_ROOT_CA);
      HTTPClient http;String base=SUPABASE_URL;while(base.endsWith("/"))base.remove(base.length()-1);
      if(http.begin(tls,base+"/functions/v1/esp32-api/"+req->path)) {
        http.setTimeout((req->audio || req->kind==NetworkKind::Speech || req->kind==NetworkKind::Action)?45000:12000);
        http.addHeader("apikey",SUPABASE_PUBLISHABLE_KEY);
        if(req->token.length())http.addHeader("Authorization","Bearer "+req->token);
        http.addHeader("Content-Type",req->audio?"audio/wav":"application/json");
        result->status=req->audio?http.POST(req->audio,req->bytes):req->method=="GET"?http.GET():http.POST(req->body);
        if(result->status>0) {
          if(req->kind==NetworkKind::Speech && result->status==200) {
            AudioResponse response; int received=http.writeToStream(&response);
            if(received<0 || response.overflow || !response.size || response.size%2)result->status=-4;
            else {result->audio=response.data;result->bytes=response.size;response.data=nullptr;}
          } else {
          BoundedResponse response;
          int received=http.writeToStream(&response);
          if(received<0 || response.overflow)result->status=-4;
          else result->body=std::move(response.body);
          }
        }
        http.end();
      }
    }
#endif
    delete req;
    // Only the main loop applies results or accesses screen/NVS/I2C state.
    xQueueSend(results,&result,portMAX_DELAY);
  }
}
}
bool apiConfigured() {
#if defined(SUPABASE_URL) && defined(SUPABASE_PUBLISHABLE_KEY) && defined(SUPABASE_ROOT_CA)
  return String(SUPABASE_URL).length() && String(SUPABASE_PUBLISHABLE_KEY).length() && String(SUPABASE_ROOT_CA).length();
#else
  return false;
#endif
}
bool networkBegin() {
  requests=xQueueCreate(4,sizeof(NetworkRequest*));results=xQueueCreate(4,sizeof(NetworkResult*));
  if(!requests || !results)return false;
  started=xTaskCreate(worker,"network",12288,nullptr,1,nullptr)==pdPASS;
  if(!started){vQueueDelete(requests);vQueueDelete(results);requests=results=nullptr;}
  return started;
}
bool networkSubmit(NetworkRequest* req){if(!started)return false;if(wakeHook)wakeHook();++pending;if(xQueueSend(requests,&req,0)==pdTRUE)return true;--pending;return false;}
NetworkResult* networkPoll(){NetworkResult* r=nullptr;if(results)xQueueReceive(results,&r,0);if(r)--pending;return r;}

bool networkBusy(){return pending.load()!=0;}
void networkWakeHook(void (*hook)()){wakeHook=hook;}
