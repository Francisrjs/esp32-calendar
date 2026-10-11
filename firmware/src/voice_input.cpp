#include "voice_input.h"
#include "input_rules.h"
#include <Wire.h>
#include <driver/i2s.h>
#include <atomic>
#include <cmath>
#include <cstring>
extern "C" {
#include <device/include/es7210_adc.h>
#include <device/include/es8311_codec.h>
}

namespace {
constexpr size_t sampleRate = 16000, maxSamples = sampleRate * 12;
QueueHandle_t events = nullptr;
std::atomic<bool> powerWanted{true}, powerOn{true}, powerFailed{false};
std::atomic<bool> enabled{true}, requested{false}, busy{false}, ready{false};
bool initialized=false;
std::atomic<bool> held{false};
QueueHandle_t playback=nullptr;
struct Playback {uint8_t* pcm;size_t bytes;};
std::atomic<bool> soundRequested{false};
std::atomic<int> level{0}, peakLevel{0};
std::atomic<int> channelPeak[4];
std::atomic<int> intervalPeak[4];
std::atomic<bool> activityWake{true}, automaticAllowed{true};
std::atomic<int> threshold{500};
uint8_t* wav = nullptr;
size_t wavBytes = 0;
const audio_codec_if_t* codec = nullptr;
const audio_codec_if_t* speaker = nullptr;
audio_codec_ctrl_if_t ctrl = {};
audio_codec_ctrl_if_t speakerCtrl = {};
int readReg(const audio_codec_ctrl_if_t* control, int reg, int, void* data, int len) {
  const uint8_t address=control==&speakerCtrl?0x18:0x40;
  Wire.beginTransmission(address); Wire.write(reg);
  if (Wire.endTransmission(false)) return -1;
  if (Wire.requestFrom(address, uint8_t(len)) != len) return -1;
  for (int i=0;i<len;++i) static_cast<uint8_t*>(data)[i]=Wire.read();
  return 0;
}
int writeReg(const audio_codec_ctrl_if_t* control, int reg, int, void* data, int len) {
  Wire.beginTransmission(control==&speakerCtrl?0x18:0x40); Wire.write(reg);
  Wire.write(static_cast<uint8_t*>(data), len); return Wire.endTransmission();
}
bool isOpen(const audio_codec_ctrl_if_t*) { return true; }
int gpioSetup(int16_t pin,audio_gpio_dir_t,audio_gpio_mode_t) { pinMode(pin,OUTPUT); return 0; }
int gpioSet(int16_t pin,bool high) { digitalWrite(pin,high); return 0; }
bool gpioGet(int16_t pin) { return digitalRead(pin); }
const audio_codec_gpio_if_t speakerGpio={gpioSetup,gpioSet,gpioGet};
bool tone() {
  if(!speaker) return false;
  int16_t data[1024];
  for(int block=0;block<16;++block) {
    for(int i=0;i<256;++i) {
      const int sample=int(1507*sin(2*3.141592653589793*880*(block*256+i)/sampleRate));
      for(int channel=0;channel<4;++channel) data[4*i+channel]=sample;
    }
    size_t written=0;
    if(i2s_write(I2S_NUM_0,data,sizeof(data),&written,pdMS_TO_TICKS(100))!=ESP_OK || written!=sizeof(data))return false;
  }
  return true;
}
void notify(VoiceEvent event) { xQueueSend(events, &event, 0); }
void u16(size_t at, uint16_t n) { wav[at]=n; wav[at+1]=n>>8; }
void u32(size_t at, uint32_t n) { for(int i=0;i<4;++i) wav[at+i]=n>>(8*i); }
void header(size_t count) {
  memcpy(wav,"RIFF",4); u32(4,36+count*2); memcpy(wav+8,"WAVEfmt ",8);
  u32(16,16); u16(20,1); u16(22,1); u32(24,sampleRate);
  u32(28,sampleRate*2); u16(32,2); u16(34,16);
  memcpy(wav+36,"data",4); u32(40,count*2); wavBytes=44+count*2;
}
void captureTask(void*) {
  int16_t stereo[1280];
  static int16_t preRoll[4800]={}; size_t prePosition=0;
  VoiceGate gate;
  unsigned activeFrames=0;
  float noise=100;
  uint32_t firstClap=0, transientAt=0, cooldown=millis()+2000;
  uint32_t captureStart=0, lastSpeech=0, speechFrames=0;
  size_t samples=0;
  bool capturing=false, previousLoud=false;
  uint32_t muteUntil=0;
  while (true) {
    if(powerWanted.load()!=powerOn.load() && !capturing && !ready.load() && !busy.load() && !held.load() && !requested.load() && !soundRequested.load() && uxQueueMessagesWaiting(playback)==0) {
      bool on=powerWanted.load();int err=0;
      if(on) {
        err=i2s_start(I2S_NUM_0);
        if(!err)err=codec->enable(codec,true);
        if(!err && speaker)err=speaker->enable(speaker,true);
        if(!err && speaker)err=speaker->set_vol(speaker,0);
        if(!err && speaker)err=speaker->mute(speaker,false);
        if(!err)i2s_zero_dma_buffer(I2S_NUM_0);
      } else {
        if(speaker)err=speaker->mute(speaker,true);
        if(!err && speaker)err=speaker->enable(speaker,false);
        digitalWrite(46,LOW);
        if(!err)err=codec->enable(codec,false);
        if(!err)err=i2s_stop(I2S_NUM_0);
      }
      if(!err){powerOn=on;level=0;gate.reset();memset(preRoll,0,sizeof(preRoll));prePosition=0;cooldown=millis()+2000;firstClap=0;previousLoud=false;}
      else {powerFailed=true;powerWanted=powerOn.load();Serial.printf("power audio transition failed: %d\n",err);}
      Serial.printf("power audio: %s\n",powerOn.load()?"running":"stopped");
    }
    if(!powerOn.load()){vTaskDelay(pdMS_TO_TICKS(20));continue;}

    if(!capturing && !ready.load() && soundRequested.exchange(false)) {
      bool played=tone();
      muteUntil=millis()+1000;cooldown=muteUntil;
      gate.reset();activeFrames=0;memset(preRoll,0,sizeof(preRoll));prePosition=0;
      Serial.printf("audio chime: %s; microphone guard=1000 ms\n",played?"I2S OK":"FAILED");
    }
    Playback play;
    if(!capturing && !ready.load() && xQueueReceive(playback,&play,0)==pdTRUE) {
      bool success=true; int16_t output[1024];
      int speechPeak=0;
      for(size_t at=0;at<play.bytes;at+=2){int16_t sample;memcpy(&sample,play.pcm+at,2);speechPeak=max(speechPeak,abs(int(sample)));}
      // Voice targets 65% of PCM full scale; DAC runs at unity gain.
      // Bound normalization for exceptionally quiet provider output.
      const float speechGain=speechPeak?min(8.0f,21298.0f/speechPeak):1.0f;
      Serial.printf("speech gain: %.2fx peak=%d\n",speechGain,speechPeak);
      for(size_t offset=0;offset<play.bytes && success;) {
        size_t count=min(size_t(256),(play.bytes-offset)/2);
        for(size_t i=0;i<count;++i){int16_t value;memcpy(&value,play.pcm+offset+i*2,2);for(int ch=0;ch<4;++ch)output[i*4+ch]=int16_t(value*speechGain);}
        size_t written=0;
        success=i2s_write(I2S_NUM_0,output,count*8,&written,pdMS_TO_TICKS(200))==ESP_OK && written==count*8;
        offset+=count*2;
      }
      // Drain the last TX DMA buffers before clearing them or playing the ready cue.
      vTaskDelay(pdMS_TO_TICKS(150));
      if(success)success=tone();
      vTaskDelay(pdMS_TO_TICKS(150));i2s_zero_dma_buffer(I2S_NUM_0);
      Serial.printf("speech playback: %s bytes=%u microphone guard=1000 ms\n",success?"OK":"FAILED",unsigned(play.bytes));
      free(play.pcm);muteUntil=millis()+1000;cooldown=muteUntil;
      gate.reset();memset(preRoll,0,sizeof(preRoll));prePosition=0;requested=false;busy=false;
      notify(success?VoiceEvent::PlaybackDone:VoiceEvent::PlaybackError);
    }
    size_t bytes=0;
    if (i2s_read(I2S_NUM_0,stereo,sizeof(stereo),&bytes,pdMS_TO_TICKS(100)) != ESP_OK || !bytes) {
      vTaskDelay(pdMS_TO_TICKS(20)); continue;
    }
    // A gate observation is always exactly 320 mono samples (20 ms).
    while(bytes<sizeof(stereo)) {
      size_t more=0;
      if(i2s_read(I2S_NUM_0,reinterpret_cast<uint8_t*>(stereo)+bytes,sizeof(stereo)-bytes,&more,pdMS_TO_TICKS(100))!=ESP_OK || !more)break;
      bytes+=more;
    }
    if(bytes!=sizeof(stereo)){gate.reset();continue;}
    const size_t frames=bytes/8;
    int peaks[4]={};
    for(size_t i=0;i<frames;++i) for(int ch=0;ch<4;++ch) peaks[ch]=max(peaks[ch],abs(int(stereo[4*i+ch])));
    for(int ch=0;ch<4;++ch) {
      channelPeak[ch]=peaks[ch];
      int previous=intervalPeak[ch].load();
      while(previous<peaks[ch] && !intervalPeak[ch].compare_exchange_weak(previous,peaks[ch])) {}
    }
    double power=0; int peak=0;
    for(size_t i=0;i<frames;++i) {
      // Manufacturer TDM mapping: channel 0 is microphone, channel 1 DAC reference.
      const int sample=stereo[4*i];
      stereo[i]=sample; power+=double(sample)*sample; peak=max(peak,abs(sample));
    }
    float rms=sqrt(power/max(size_t(1),frames)); level=rms; peakLevel=peak;
    uint32_t now=millis();
    if(muteUntil && int32_t(now-muteUntil)<0){gate.reset();continue;}
    muteUntil=0;
    if (ready.load() || held.load()) {gate.reset();continue;} // main owns the immutable WAV while HTTPS runs
    if (!capturing) {
      for(size_t i=0;i<frames;++i) { preRoll[prePosition]=stereo[i]; prePosition=(prePosition+1)%4800; }
      if ((!enabled.load() || !automaticAllowed.load()) && !requested.load()) {gate.reset();activeFrames=0;continue;}
      const bool loud=rms>max(650.0f,noise*4.0f) && peak>2500 && peak>rms*1.8f;
      bool manual=requested.exchange(false), wake=manual;
      const bool sustained=gate.update(rms,threshold.load());activeFrames=gate.consecutive;
      const bool automatic=enabled.load() && automaticAllowed.load();
      if(automatic && activityWake.load() && sustained && int32_t(now-cooldown)>=0) wake=true;
      if (automatic && !activityWake.load() && int32_t(now-cooldown)>=0 && loud && !previousLoud && now-transientAt>150) {
        transientAt=now;
        if(firstClap && now-firstClap>=180 && now-firstClap<=850) { wake=true; firstClap=0; }
        else firstClap=now;
      }
      previousLoud=loud;
      if(!loud && activeFrames==0) noise=noise*.99f+min(rms,1500.0f)*.01f;
      if(firstClap && now-firstClap>850) firstClap=0;
      if(wake) {
        Serial.printf("voice trigger: source=%s rms=%.1f threshold=%d consecutive=%u peak=%d\n",manual?"manual":activityWake.load()?"rms":"clap",rms,threshold.load(),activeFrames,peak);
        busy=true; capturing=true; samples=0; speechFrames=0; lastSpeech=0;
        captureStart=now; notify(VoiceEvent::Listening);
        if(activityWake.load()) {
          for(size_t i=0;i<4800;++i) memcpy(wav+44+i*2,&preRoll[(prePosition+i)%4800],2);
          samples=4800; speechFrames=manual?0:3; lastSpeech=now;
        } else tone(); // Voice wake skips a tone that would cover the first words.
        activeFrames=0;gate.reset();
      }
      continue;
    }
    // Ignore clap decay before keeping speech. Maximum recording is twelve seconds.
    if(!activityWake.load() && now-captureStart<350) continue;
    size_t count=min(frames,maxSamples-samples);
    memcpy(wav+44+samples*2,stereo,count*2); samples+=count;
    if(rms>threshold.load()) { lastSpeech=now; ++speechFrames; }
    const bool heardSpeech=speechFrames>=3;
    if(samples>=maxSamples || captureFinished(now-captureStart,heardSpeech,now-lastSpeech)) {
      Serial.printf("voice capture: duration=%lu ms samples=%u speech_blocks=%lu silence=%lu ms result=%s\n",static_cast<unsigned long>(now-captureStart),unsigned(samples),static_cast<unsigned long>(speechFrames),static_cast<unsigned long>(now-lastSpeech),heardSpeech?"upload":"discard");
      capturing=false; cooldown=now+2500; previousLoud=false; firstClap=0;
      if(heardSpeech) { header(samples); ready=true; notify(VoiceEvent::Ready); }
      else { busy=false; notify(VoiceEvent::NoSpeech); }
    }
  }
}
}

bool voiceBegin() {
  // Codec configuration happens once on main, before any concurrent I2C work.
  ctrl.read_reg=readReg; ctrl.write_reg=writeReg; ctrl.is_open=isOpen;
  i2s_config_t config={}; config.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_RX|I2S_MODE_TX);
  config.sample_rate=sampleRate; config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format=I2S_CHANNEL_FMT_MULTIPLE;
  config.chan_mask=i2s_channel_t(I2S_TDM_ACTIVE_CH0|I2S_TDM_ACTIVE_CH1|I2S_TDM_ACTIVE_CH2|I2S_TDM_ACTIVE_CH3);
  config.total_chan=4;
  config.communication_format=I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1; config.dma_buf_count=6; config.dma_buf_len=320;
  config.use_apll=false; config.fixed_mclk=sampleRate*256;
  config.tx_desc_auto_clear=true;
  i2s_pin_config_t pins={}; pins.mck_io_num=16; pins.bck_io_num=9;
  pins.ws_io_num=45; pins.data_out_num=8; pins.data_in_num=10;
  if(i2s_driver_install(I2S_NUM_0,&config,0,nullptr)!=ESP_OK) return false;
  if(i2s_set_pin(I2S_NUM_0,&pins)!=ESP_OK) { i2s_driver_uninstall(I2S_NUM_0); return false; }
  es7210_codec_cfg_t cfg={}; cfg.ctrl_if=&ctrl;
  cfg.mic_selected=ES7120_SEL_MIC1|ES7120_SEL_MIC2|ES7120_SEL_MIC3|ES7120_SEL_MIC4;
  cfg.mclk_div=256; codec=es7210_codec_new(&cfg);
  esp_codec_dev_sample_info_t fs={}; fs.sample_rate=sampleRate; fs.channel=4; fs.bits_per_sample=16;
  if(!codec || codec->set_fs(codec,&fs)!=0 || codec->enable(codec,true)!=0 || codec->set_mic_gain(codec,30)!=0) {
    i2s_driver_uninstall(I2S_NUM_0); return false;
  }
  speakerCtrl=ctrl;
  es8311_codec_cfg_t out={}; out.ctrl_if=&speakerCtrl; out.gpio_if=&speakerGpio;
  out.codec_mode=ESP_CODEC_DEV_WORK_MODE_DAC; out.pa_pin=46; out.use_mclk=true; out.mclk_div=256;
  speaker=es8311_codec_new(&out);
  fs.channel=2;
  if(speaker && (speaker->set_fs(speaker,&fs)!=0 || speaker->enable(speaker,true)!=0 || speaker->set_vol(speaker,0)!=0 || speaker->mute(speaker,false)!=0)) speaker=nullptr;
  i2s_zero_dma_buffer(I2S_NUM_0);
  wav=static_cast<uint8_t*>(ps_malloc(44+maxSamples*2));
  events=xQueueCreate(4,sizeof(VoiceEvent));
  playback=xQueueCreate(1,sizeof(Playback));
  if(wav && events && playback)initialized=xTaskCreatePinnedToCore(captureTask,"voice-input",8192,nullptr,2,nullptr,0)==pdPASS;
  if(!initialized){if(events)vQueueDelete(events);events=nullptr;free(wav);wav=nullptr;i2s_driver_uninstall(I2S_NUM_0);}
  return initialized;
}
bool voicePoll(VoiceEvent& event) { return events && xQueueReceive(events,&event,0)==pdTRUE; }
bool voiceListen() { if(!initialized || busy.load() || held.load() || !voiceSetPower(true)) return false; requested=true; return true; }
void voiceEnable(bool value) { enabled=value; }
void voiceActivity(bool value) { activityWake=value; }
int voiceLevel() { return level.load(); }
void voiceStatus() {
  Serial.printf("audio: ready=%s wake=%s enabled=%s busy=%s rms=%d threshold=%d allowed=%s peak=%d channels=[%d,%d,%d,%d] max=[%d,%d,%d,%d]\n",events?"yes":"no",activityWake.load()?"voice":"clap",enabled.load()?"on":"off",busy.load()?"yes":"no",level.load(),threshold.load(),automaticAllowed.load()?"yes":"no",peakLevel.load(),channelPeak[0].load(),channelPeak[1].load(),channelPeak[2].load(),channelPeak[3].load(),intervalPeak[0].exchange(0),intervalPeak[1].exchange(0),intervalPeak[2].exchange(0),intervalPeak[3].exchange(0));
}
void voiceRegisters() {
  const uint8_t registers[]={0,1,2,4,6,7,8,0x11,0x12,0x40,0x43,0x44,0x45,0x46,0x4b,0x4c};
  for(uint8_t reg:registers) { uint8_t value=0; int result=readReg(&ctrl,reg,1,&value,1); Serial.printf("ES7210 reg %02X=%02X status=%d\n",reg,value,result); }
  Serial.printf("speaker: initialized=%s PA46=%d output=GPIO8 volume=0dB speech target=65%%\n",speaker?"yes":"no",digitalRead(46));
  const uint8_t dacRegisters[]={0,1,2,6,7,8,9,0x0d,0x12,0x31,0x32,0x37};
  for(uint8_t reg:dacRegisters){uint8_t value=0;int result=readReg(&speakerCtrl,reg,1,&value,1);Serial.printf("ES8311 reg %02X=%02X status=%d\n",reg,value,result);}
}
uint8_t* voiceWav(size_t& bytes) { bytes=ready.load()?wavBytes:0; return bytes?wav:nullptr; }
void voiceRelease() { wavBytes=0; ready=false; busy=false; }

void voiceConfigure(int value) { if(value==500 || value==800 || value==1200)threshold=value; }
void voiceAllowAutomatic(bool value) { automaticAllowed=value; }
bool voiceSound() {
  if(!initialized || !speaker || busy.load() || !voiceSetPower(true))return false;
  soundRequested=true;return true;
}

void voiceHold(bool value){held=value;}
bool voiceOccupied(){return busy.load() || held.load();}
bool voicePlay(uint8_t* pcm,size_t bytes){
  if(!initialized || !speaker || !pcm || !bytes || bytes%2 || !voiceSetPower(true) || busy.exchange(true))return false;
  Playback play{pcm,bytes};
  if(xQueueSend(playback,&play,0)!=pdTRUE){busy=false;return false;}
  held=false;return true;
}

bool voicePowered(){return initialized && powerOn.load();}
bool voiceSetPower(bool on,uint32_t timeoutMs) {
  if(!initialized)return !on;
  powerFailed=false;powerWanted=on;uint32_t start=millis();
  while(powerOn.load()!=on && !powerFailed.load() && millis()-start<timeoutMs)delay(5);
  bool success=powerOn.load()==on && !powerFailed.load();
  if(!success)powerWanted=powerOn.load();
  return success;
}
