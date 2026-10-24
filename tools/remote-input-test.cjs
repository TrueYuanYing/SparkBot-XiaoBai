// Execute the actual embedded page script with browser/network boundary adapters.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const html=fs.readFileSync(__dirname+'/../firmware/main/boards/esp-sparkbot/assets/hub.html','utf8');
const script=html.split('<script>')[1].split('</script>')[0];
let now=1000;
const elements=new Map(),sent=[],windowEvents={};
function element(){return {value:'40',style:{},dataset:{},children:[],listeners:{},classList:{add(){},remove(){},toggle(){}},setAttribute(){},removeAttribute(){},append(v){this.children.push(v)},addEventListener(n,f){this.listeners[n]=f},setPointerCapture(){},getBoundingClientRect(){return {left:0,top:0,width:200,height:200}}};}
const document={hidden:true,documentElement:element(),getElementById(id){if(!elements.has(id))elements.set(id,element());return elements.get(id)},querySelectorAll(){return []},createElement:element,addEventListener(){}};
const context=vm.createContext({document,window:{addEventListener(name,callback){windowEvents[name]=callback},scrollTo(){}},localStorage:{getItem(){return null}},setTimeout(){},clearTimeout(){},setInterval(){},performance:{now:()=>now},Date:class extends Date{static now(){return now}},WebSocket:{OPEN:1},location:{host:'test.invalid',hostname:'test.invalid'},fakeSocket:{readyState:1,bufferedAmount:0,send(s){sent.push(JSON.parse(s))},close(){this.readyState=3}},console});
vm.runInContext(script,context);
vm.runInContext('ws=fakeSocket;held=true;pointer=7;lastAck=Date.now()',context);
for(let i=0;i<200;i++){now=1000+i*2;vm.runInContext('drag({clientX:145,clientY:55})',context)}
assert(sent.length<=9,`Pointer events flooded ${sent.length} movement messages in 400 ms`);
let last=sent.at(-1);assert(last.x>0&&last.y>0);
vm.runInContext('release()',context);
assert.equal(sent.at(-1).x,0);assert.equal(sent.at(-1).y,0);
assert(sent.at(-1).seq>last.seq,'Release must send immediately even inside throttle window');
console.log('PASS: 500 Hz pointer input is limited to 20 Hz; release sends immediate stop');
vm.runInContext('held=true;pointer=7',context);
document.getElementById('pad').listeners.pointerdown({pointerId:9,clientX:60,clientY:40,preventDefault(){}});
assert.equal(vm.runInContext('pointer',context),7,'A second finger must not steal the steering pointer');
document.getElementById('pad').listeners.pointerup({pointerId:9});
assert.equal(vm.runInContext('held',context),true,'Lifting a second finger must not interrupt steering');
document.getElementById('pad').listeners.pointerup({pointerId:7});
assert.equal(vm.runInContext('held',context),false);
assert.equal(sent.at(-1).x,0);assert.equal(sent.at(-1).y,0);
console.log('PASS: only the active finger can move/release the joystick');
vm.runInContext('held=true;pointer=7;vector={x:.4,y:.4}',context);
assert.equal(typeof windowEvents.resize,'function','Rotating/resizing needs a release handler');
windowEvents.resize();
assert.equal(vm.runInContext('held',context),false);assert.equal(sent.at(-1).x,0);assert.equal(sent.at(-1).y,0);
console.log('PASS: orientation/viewport changes release the drive command');
