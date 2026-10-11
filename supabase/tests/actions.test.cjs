// Runs the actual TypeScript handlers with in-memory persistence and provider doubles.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('../../front-end/node_modules/typescript');
const crypto = require('node:crypto').webcrypto;
const root = path.resolve(__dirname, '../functions/esp32-api');
let interpretation, providerCalls=0, calendarConnected=true, calendarWrites=0;
const events=[];
async function fetchDouble(url, options={}) {
  if(url.includes('/responses')) {
    ++providerCalls;
    return Response.json({output:[{content:[{type:'output_text',text:JSON.stringify(interpretation)}]}]});
  }
  if(options.method==='DELETE'){calendarWrites++;events.splice(events.findIndex(e=>url.endsWith('/'+e.id)),1);return new Response(null,{status:204});}
  if(options.method==='POST'){calendarWrites++;const event={...JSON.parse(options.body),id:'evt1',etag:'v1'};events.push(event);return Response.json(event);}
  if(options.method==='PATCH'){calendarWrites++;return Response.json({ok:true});}
  return Response.json({items:events});
}
const loaded={};
function load(name) {
  if(loaded[name])return loaded[name];
  const exports={};loaded[name]=exports;
  const code=ts.transpileModule(fs.readFileSync(path.join(root,name),'utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2022}}).outputText;
  vm.runInNewContext(code,{exports,require:p=>load(path.basename(p)),Deno:{env:{get:()=> 'test-key'}},crypto,fetch:fetchDouble,Response,Request,URLSearchParams,TextEncoder,AbortSignal,console:{log(){},error(){}}});
  return exports;
}
const rules=load('action_rules.ts');const {handleActions}=load('actions.ts');
const tables={esp32_action_requests:[],esp32_action_context:[],esp32_tasks:[],esp32_commands:[],esp32_device_settings:[{device_id:'device1',owner_id:'owner1',revision:1}],esp32_device_alarms:[]};
let serial=0;
class Query {
  constructor(table){this.table=table;this.filters=[];this.mode='select';this.count=Infinity;}
  select(){return this;} eq(k,v){this.filters.push(r=>r[k]===v);return this;}
  match(values){for(const [k,v] of Object.entries(values))this.eq(k,v);return this;}
  order(){return this;} limit(n){this.count=n;return this;}
  insert(row){this.mode='insert';this.row=row;return this;}
  upsert(row){this.mode='upsert';this.row=row;return this;}
  update(row){this.mode='update';this.row=row;return this;}
  delete(){this.mode='delete';return this;}
  maybeSingle(){this.singleRow=true;return this;} single(){this.singleRow=true;return this;}
  then(resolve,reject){return Promise.resolve().then(()=>this.execute()).then(resolve,reject);}
  execute(){
    const rows=tables[this.table];let selected=rows.filter(r=>this.filters.every(f=>f(r))).slice(0,this.count);
    if(this.mode==='insert'){
      if(this.table==='esp32_action_requests' && rows.some(r=>r.request_id===this.row.request_id || (r.device_id===this.row.device_id && r.status==='running')))return {error:{code:'23505'}};
      const row={id:'id'+(++serial),revision:1,created_at:new Date().toISOString(),status:this.table==='esp32_action_requests'?'running':'pending',...this.row};rows.push(row);selected=[row];
    }
    if(this.mode==='upsert'){const old=rows.find(r=>r.device_id===this.row.device_id);if(old)Object.assign(old,this.row);else rows.push({...this.row});selected=[this.row];}
    if(this.mode==='update')for(const r of selected)Object.assign(r,this.row);
    if(this.mode==='delete')for(const r of selected)rows.splice(rows.indexOf(r),1);
    return {error:null,data:this.singleRow?selected[0]??null:selected};
  }
}
const db={from:t=>new Query(t),rpc:async(name,args)=>{
  if(name==='esp32_reserve_voice')return {data:true,error:null};
  let task;
  if(name==='esp32_create_task'){task={id:'task'+(++serial),owner_id:'owner1',title:args.p_title,due_date:args.p_due_date,status:'pending',revision:1};tables.esp32_tasks.push(task);}
  else task=tables.esp32_tasks.find(t=>t.id===args.p_task_id&&t.owner_id==='owner1');
  if(name==='esp32_complete_task')task.status='completed';
  const result={task};tables.esp32_commands.push({owner_id:'owner1',device_id:'device1',request_id:args.p_request_id,status:'succeeded',result});return {data:result,error:null};
}};
const deps={db,authenticate:async()=>({id:'device1',owner_id:'owner1'}),calendar:async()=>{if(!calendarConnected)throw Error('not connected');return {token:'test',calendarId:'primary'};},json:(v,status=200)=>Response.json(v,{status})};
let requestNo=0;const conv='11111111-1111-4111-8111-111111111111';
function intent(action,args={}){return {action,...Object.fromEntries(rules.intentSchema.required.filter(k=>k!=='action').map(k=>[k,null])),...args};}
async function run(text,action,args={},options={}) {
  interpretation=intent(action,args);
  const body={request_id:`00000000-0000-4000-8000-${String(++requestNo).padStart(12,'0')}`,conversation_id:conv,text,clock_valid:true,local_time:'2026-10-10 15:00',...options};
  const response=await handleActions(new Request('https://test/action-run',{method:'POST',body:JSON.stringify(body)}),'action-run',deps);
  return {result:await response.json(),body};
}
(async()=>{
  assert.equal(rules.validDate('2026-02-30'),false);
  assert.equal(rules.validTime('24:00'),false);
  assert.equal(rules.localDay(new Date('2026-10-11T02:59:00Z')),'2026-10-10');
  assert.equal(rules.localDay(new Date('2026-10-11T03:00:00Z')),'2026-10-11');
  assert.equal(rules.validIntent(intent('task.create',{duration:0})),false);
  assert.equal(rules.preference('owner_id','other'),null);
  let r=await run('agregá leche','task.create',{title:'Leche',date:'2026-10-11'});assert.equal(r.result.status,'succeeded');
  const ledger=tables.esp32_action_requests.find(q=>q.request_id===r.body.request_id);ledger.status='running';ledger.result=null;
  let recovered=await handleActions(new Request('https://test',{method:'POST',body:JSON.stringify(r.body)}),'action-status',deps);assert.equal((await recovered.json()).status,'succeeded');
  const count=tables.esp32_tasks.length;let response=await handleActions(new Request('https://test',{method:'POST',body:JSON.stringify(r.body)}),'action-run',deps);assert.equal((await response.json()).entity_id,r.result.entity_id);assert.equal(tables.esp32_tasks.length,count);
  response=await handleActions(new Request('https://test',{method:'POST',body:JSON.stringify({...r.body,text:'otra tarea'})}),'action-run',deps);assert.equal(response.status,409);
  tables.esp32_tasks.push({id:'foreign',owner_id:'owner2',title:'Secreto',status:'pending'});
  r=await run('pendientes','task.list');assert(!r.result.display_text.includes('Secreto'));
  await run('otra leche','task.create',{title:'Leche',date:'2026-10-12'});
  r=await run('completá leche','task.complete',{target:'Leche'});assert.equal(r.result.status,'needs_clarification');assert.equal(tables.esp32_tasks.filter(t=>t.status==='completed').length,0);
  r=await run('la primera','task.complete');assert.equal(r.result.status,'succeeded');assert.equal(tables.esp32_tasks.filter(t=>t.status==='completed').length,1);
  r=await run('tarea mañana','task.create',{title:'Sin reloj',date:'2026-10-11'},{clock_valid:false});assert.equal(r.result.status,'needs_clarification');assert(!tables.esp32_tasks.some(t=>t.title==='Sin reloj'));
  const providerBefore=providerCalls;r=await run('Subtítulos por la comunidad','task.create',{title:'Bad'});assert.equal(r.result.status,'unsupported');assert.equal(providerCalls,providerBefore);
  r=await run('hora','clock.read',{}, {clock_valid:false});assert.equal(r.result.status,'failed');
  r=await run('temperatura','environment.read',{}, {temperature:20,humidity:45,sensor_age_seconds:100});assert.equal(r.result.status,'failed');
  r=await run('temperatura','environment.read',{}, {temperature:20,humidity:45,sensor_age_seconds:10});assert.equal(r.result.status,'succeeded');
  r=await run('agenda','view.change',{view:'calendar'});assert.equal(r.result.local.view,'calendar');
  r=await run('abrir el día uno con tareas','view.change',{view:'daily',date:'2026-10-05'});assert.equal(r.result.local.view,'daily');assert.equal(new Date(r.result.local.date+'T12:00:00Z').getUTCDay(),1);assert(r.result.refresh.includes('calendar'));
  r=await run('Abrir reposo diario','view.change',{view:'rest'});assert.equal(r.result.local.view,'daily');
  r=await run('reposo semanal','view.change',{view:'weekly'});assert.equal(r.result.local.view,'weekly');assert.equal(r.result.local.date,null);
  r=await run('abrir día','view.change',{view:'daily',date:'2026-02-30'});assert.equal(r.result.status,'needs_clarification');
  r=await run('desactivar escucha','settings.update',{setting:'auto_voice',value:'false'});assert.equal(r.result.settings.auto_voice,false);assert.equal(tables.esp32_device_settings[0].auto_voice,false);
  r=await run('música','music.control');assert.equal(r.result.status,'unsupported');
  calendarConnected=false;r=await run('agenda mañana','calendar.list',{date:'2026-10-11'});assert.equal(r.result.error_code,'calendar_not_connected');calendarConnected=true;
  r=await run('crear reunión','calendar.create',{title:'Reunión',date:'2026-10-11',time:'10:00'});assert.equal(r.result.status,'needs_clarification');assert.equal(calendarWrites,0);
  r=await run('una hora','calendar.create',{title:'Reunión',date:'2026-10-11',time:'10:00',duration:60});assert.equal(r.result.status,'succeeded');assert.equal(calendarWrites,1);
  r=await run('eliminar reunión','calendar.delete',{target:'Reunión',date:'2026-10-11'});assert.equal(r.result.status,'needs_confirmation');assert.equal(calendarWrites,1);
  r=await run('sí','calendar.delete');assert.equal(r.result.status,'succeeded');assert.equal(calendarWrites,2);
  r=await run('sí','calendar.delete');assert.equal(r.result.error_code,'no_pending_confirmation');
  events.push({id:'evt2',etag:'v1',summary:'Prueba',start:{dateTime:'2026-10-11T10:00:00-03:00'},end:{dateTime:'2026-10-11T11:00:00-03:00'}});
  r=await run('borrar prueba','calendar.delete',{target:'Prueba'});assert.equal(r.result.status,'needs_confirmation');tables.esp32_action_context[0].expires_at='2000-01-01';
  r=await run('sí','calendar.delete');assert.equal(r.result.error_code,'no_pending_confirmation');assert.equal(events.length,1);
  r=await run('borrar prueba','calendar.delete',{target:'Prueba'});
  r=await run('sí','calendar.delete',{}, {conversation_id:'22222222-2222-4222-8222-222222222222'});assert.equal(r.result.error_code,'no_pending_confirmation');assert.equal(events.length,1);
  console.log('PASS: dates at midnight, validation, idempotency/conflict, owner isolation, clarification, noise rejection, sensor freshness, preferences, unavailable integrations, calendar duration and scoped/expired confirmation.');
})().catch(e=>{console.error(e);process.exitCode=1;});
