'use strict';
const $ = (s, root = document) => root.querySelector(s);
const $$ = (s, root = document) => [...root.querySelectorAll(s)];
const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
const menu = $('.menu-toggle');
menu.addEventListener('click', () => { const open = menu.getAttribute('aria-expanded') !== 'true'; menu.setAttribute('aria-expanded', String(open)); $('#nav').classList.toggle('open', open); });
$$('#nav a').forEach(a => a.addEventListener('click', () => { menu.setAttribute('aria-expanded', 'false'); $('#nav').classList.remove('open'); }));
let scrollQueued = false;
function updateScroll() { const max = document.documentElement.scrollHeight - innerHeight; $('.scroll-progress').style.width = `${max > 0 ? scrollY / max * 100 : 0}%`; scrollQueued = false; }
addEventListener('scroll', () => { if (!scrollQueued) { requestAnimationFrame(updateScroll); scrollQueued = true; } }, { passive: true });
addEventListener('resize', updateScroll); updateScroll();
if ('IntersectionObserver' in window) {
 const observer = new IntersectionObserver(entries => entries.forEach(e => { if (e.isIntersecting) { $$('#nav a').forEach(a => a.classList.toggle('active', a.hash === '#' + e.target.id)); } }), { rootMargin: '-15% 0px -65% 0px' });
 $$('main>section[id]').forEach(section => observer.observe(section));
}

// An interactive browser explainer. Recipes follow wire_management; stirring,
// intro delays and inactivity are intentionally outside this simplified model.
const distance = $('#distance');
let zone = 'right';
let audio = null;
const foodTypes = {
 protein: {name:'Protein', icon:'🍗', target:140, flips:2},
 carb: {name:'Carb', icon:'🍚', target:240, flips:4},
 vegetables: {name:'Vegetables', icon:'🥦', target:320, flips:1},
 spice: {name:'Spice', icon:'🌶️', target:280, flips:3},
 oil: {name:'Oil', icon:'🫒', target:100, flips:0}
};
const dishes = [
 {name:'CHICKEN FRIED RICE', recipe:['protein','carb','spice','oil']},
 {name:'CHICKEN & BROCCOLI STIR-FRY', recipe:['protein','vegetables','spice','oil']},
 {name:'PASTA AGLIO E OLIO', recipe:['carb','spice','oil']}
];
const ingredients = new Set();
let currentFood = null, flipCount = 0, totalFlips = 0;
let roundMode = 'ready', score = 0, seconds = 60, deadline = 0, lastTick = 0, hotTime = 0, burnt = false;
let flipBusy = false, flipTimer, feedbackTimer;
const dishSelect = $('#demo-dish');
function chosenDish() { return dishes[Number(dishSelect.value)]; }
function announce(message) { $('#demo-announcement').textContent = message; }
function addScore(delta) { score = Math.max(0, score + delta); }
function renderHeat() {
 const mm = Number(distance.value), target = currentFood ? foodTypes[currentFood].target : 220;
 const next = mm < target - 40 ? 'high' : mm > target + 40 ? 'low' : 'right';
 const config = { high:['TOO MUCH','#ff8150','♨'], low:['NOT ENOUGH','#83cafa','❄'], right:['JUST RIGHT','#9fdfbf','✳'] }[next];
 const stage = $('.demo-stage');
 stage.style.setProperty('--heat',config[1]); stage.style.setProperty('--heat-level',`${(400-mm)/350*100}%`);
 $('#heat-zone').textContent = burnt ? 'BURNT' : currentFood ? config[0] : 'WAITING';
 $('#heat-icon').textContent = config[2]; $('#distance-output').textContent = String(mm);
 distance.setAttribute('aria-valuetext',`${mm} millimeters, ${currentFood ? config[0].toLowerCase() : 'waiting for an ingredient'}`);
 if (audio) audio.setZone(currentFood && roundMode !== 'finished' ? next : 'silent');
 zone = next;
}
function renderIngredients() {
 const screen = $('#pan-ingredients'); screen.replaceChildren();
 if (!ingredients.size) { const empty=document.createElement('span');empty.className='empty-pan';empty.textContent='ADD SOMETHING GOOD';screen.append(empty); }
 else for(const id of ingredients) {const item=document.createElement('span');item.className='food';item.textContent=foodTypes[id].icon;item.title=foodTypes[id].name;screen.append(item);}
}
function renderRound() {
 const shownScore=Math.floor(score), active=roundMode==='cooking';
 $('#round-state').textContent = active ? 'COOKING / LIVE' : roundMode==='finished' ? 'ROUND COMPLETE' : 'READY TO COOK';
 $('#matrix-dish').textContent=chosenDish().name;
 $('#matrix-score').textContent=`SCORE ${String(shownScore).padStart(3,'0')} · ${seconds}s`;
 $('#game-matrix').setAttribute('aria-label',`${chosenDish().name}. ${active?'Cooking':roundMode==='finished'?'Round complete':'Ready'}. Score ${shownScore}. ${seconds} seconds remaining.`);
 $('#start-round').textContent=roundMode==='finished'?'Cook again':'Start cooking';
 $('#start-round').disabled=active; dishSelect.disabled=active;
 $('#finish-round').disabled=!active || seconds>30;
 $('#flip-count').textContent=String(flipCount).padStart(2,'0');
 $('#flip-help').textContent=currentFood ? `${foodTypes[currentFood].name}: ideal ${foodTypes[currentFood].flips} flips · ${totalFlips} total this round` : 'Add an ingredient to see its ideal flip count.';
 if(active) $('#round-help').textContent=seconds>30?`Finish unlocks in ${seconds-30}s. Keep cooking!`:'Finish is lit. Plate it now or use the remaining time.';
 else if(roundMode==='finished') $('#round-help').textContent=shownScore>=50?'Order up! You earned the virtual Gushers. Cook again?':'Dish served. Reach 50 points to earn the virtual Gushers.';
 else $('#round-help').textContent='60 seconds on the clock. Finish unlocks after 30 seconds.';
}
function clearPan() {
 ingredients.clear();currentFood=null;flipCount=totalFlips=0;hotTime=0;burnt=false;
 $$('[data-ingredient]').forEach(b=>b.setAttribute('aria-pressed','false'));
 clearTimeout(flipTimer);clearTimeout(feedbackTimer);flipBusy=false;$('#flip-pan').disabled=false;$('#pan-rig').classList.remove('is-flipping');$('#flip-feedback').classList.remove('flash');$('#flip-feedback').textContent='';
 renderIngredients();renderHeat();
}
function startRound() {
 clearPan();score=0;seconds=60;roundMode='cooking';lastTick=performance.now();deadline=lastTick+60000;
 matrixStarted=lastTick;renderRound();announce(`${chosenDish().name}. Cooking started. 60 seconds.`);
}
function endRound() {
 if(roundMode!=='cooking') return;
 roundMode='finished';if(audio)audio.setZone('silent');renderRound();announce(`Round complete. ${Math.floor(score)} points. ${score>=50?'Virtual Gushers earned.':'Try for 50 points next time.'}`);
}
function advanceRound(now) {
 if(roundMode!=='cooking')return;
 const dt=Math.min(500,Math.max(0,Math.min(now,deadline)-lastTick));lastTick=now;
 seconds=Math.max(0,Math.ceil((deadline-now)/1000));
 if(currentFood&&!burnt) {
  const error=Math.abs(Number(distance.value)-foodTypes[currentFood].target);
  addScore(Math.max(0,1-error/80)*dt/1000);
  hotTime=zone==='high'?hotTime+dt:0;
  if(hotTime>=3000) {burnt=true;addScore(-10);renderHeat();announce(`${foodTypes[currentFood].name} burnt. Minus 10 points.`);}
 }
 if(now>=deadline)endRound(); else renderRound();
}
$('#start-round').addEventListener('click',startRound);
$('#finish-round').addEventListener('click',()=>{advanceRound(performance.now());if(roundMode==='cooking'&&seconds<=30)endRound();});
dishSelect.addEventListener('change',()=>{if(roundMode!=='cooking'){score=0;seconds=60;roundMode='ready';clearPan();matrixStarted=performance.now();renderRound();}});
distance.addEventListener('input',()=>{advanceRound(performance.now());renderHeat();});
$$('[data-ingredient]').forEach(button=>button.addEventListener('click',()=>{
 advanceRound(performance.now());
 const id=button.dataset.ingredient,on=button.getAttribute('aria-pressed')!=='true';button.setAttribute('aria-pressed',String(on));
 if(on&&!ingredients.has(id)) {
  ingredients.add(id);currentFood=id;flipCount=0;hotTime=0;burnt=false;
  if(roundMode==='cooking'){const points=chosenDish().recipe.includes(id)?10:-10;addScore(points);announce(`${foodTypes[id].name} added. ${points>0?'Plus':'Minus'} 10 points. Heat target ${foodTypes[id].target/10} centimeters.`);}
  renderIngredients();renderHeat();renderRound();
 }
}));
$('#flip-pan').addEventListener('click',()=>{
 if(flipBusy)return;
 advanceRound(performance.now());flipBusy=true;$('#flip-pan').disabled=true;
 $('#pan-rig').classList.add('is-flipping');
 let feedback='NICE TOSS!';
 if(currentFood) {
  ++flipCount;++totalFlips;
  if(roundMode==='cooking') {const points=flipCount<=foodTypes[currentFood].flips?2:-2;addScore(points);feedback=points>0?'+2 · NICE FLIP':'−2 · OVERFLIPPED';announce(`${foodTypes[currentFood].name}, flip ${flipCount}. ${points>0?'Plus':'Minus'} 2 points.`);}
  else feedback='PRACTICE FLIP';
 } else {feedback='EMPTY PAN!';announce('Pan flipped. Add an ingredient before scoring flips.');}
 $('#flip-feedback').textContent=feedback;$('#flip-feedback').classList.remove('flash');
 // Flush the previous completed animation before starting the next score tag.
 void $('#flip-feedback').offsetWidth;$('#flip-feedback').classList.add('flash');renderRound();
 flipTimer=setTimeout(()=>{$('#pan-rig').classList.remove('is-flipping');flipBusy=false;$('#flip-pan').disabled=false;},650);
 feedbackTimer=setTimeout(()=>{$('#flip-feedback').textContent='';$('#flip-feedback').classList.remove('flash');},1000);
});
const joystick=$('#joystick'),knob=$('.joystick-knob');let sweep=0;
function setSweep(value,y=0){sweep=Math.max(-55,Math.min(55,value));$('#pan-scene').style.setProperty('--sweep',`${sweep}deg`);knob.style.transform=`translate(${sweep/55*23}px,${Math.max(-18,Math.min(18,y))}px)`;joystick.setAttribute('aria-valuenow',String(Math.round(sweep)));joystick.setAttribute('aria-valuetext',`${Math.round(sweep)} degrees of spatula sweep`);}
function drag(event){const bounds=joystick.getBoundingClientRect();setSweep((event.clientX-bounds.left-bounds.width/2)/(bounds.width/2)*55,(event.clientY-bounds.top-bounds.height/2)*.5);}
joystick.addEventListener('pointerdown',event=>{event.preventDefault();joystick.focus({preventScroll:true});joystick.setPointerCapture(event.pointerId);drag(event);});
joystick.addEventListener('pointermove',event=>{if(joystick.hasPointerCapture(event.pointerId))drag(event);});
for(const type of ['pointerup','pointercancel','lostpointercapture'])joystick.addEventListener(type,()=>setSweep(0));
joystick.addEventListener('keydown',event=>{const commands={ArrowLeft:-10,ArrowDown:-10,ArrowRight:10,ArrowUp:10};if(event.key in commands){event.preventDefault();setSweep(sweep+commands[event.key]);}else if(event.key==='Home'||event.key==='Escape'){event.preventDefault();setSweep(0);}});
$('#reset-demo').addEventListener('click',()=>{roundMode='ready';score=0;seconds=60;deadline=0;distance.value='220';clearPan();setSweep(0);matrixStarted=performance.now();renderRound();announce('Demo reset. Ready to cook.');});
// A small 5x7 font is painted as individual LEDs, so the preview stays crisp
// without an external font or library. Full dish names also appear as text.
const glyphs={
 ' ':['00000','00000','00000','00000','00000','00000','00000'],
 A:['01110','10001','10001','11111','10001','10001','10001'],B:['11110','10001','10001','11110','10001','10001','11110'],C:['01111','10000','10000','10000','10000','10000','01111'],D:['11110','10001','10001','10001','10001','10001','11110'],E:['11111','10000','10000','11110','10000','10000','11111'],F:['11111','10000','10000','11110','10000','10000','10000'],G:['01111','10000','10000','10111','10001','10001','01111'],H:['10001','10001','10001','11111','10001','10001','10001'],I:['11111','00100','00100','00100','00100','00100','11111'],J:['00111','00010','00010','00010','10010','10010','01100'],K:['10001','10010','10100','11000','10100','10010','10001'],L:['10000','10000','10000','10000','10000','10000','11111'],M:['10001','11011','10101','10101','10001','10001','10001'],N:['10001','11001','10101','10011','10001','10001','10001'],O:['01110','10001','10001','10001','10001','10001','01110'],P:['11110','10001','10001','11110','10000','10000','10000'],Q:['01110','10001','10001','10001','10101','10010','01101'],R:['11110','10001','10001','11110','10100','10010','10001'],S:['01111','10000','10000','01110','00001','00001','11110'],T:['11111','00100','00100','00100','00100','00100','00100'],U:['10001','10001','10001','10001','10001','10001','01110'],V:['10001','10001','10001','10001','10001','01010','00100'],W:['10001','10001','10001','10101','10101','10101','01010'],X:['10001','10001','01010','00100','01010','10001','10001'],Y:['10001','10001','01010','00100','00100','00100','00100'],Z:['11111','00001','00010','00100','01000','10000','11111'],
 '0':['01110','10001','10011','10101','11001','10001','01110'],'1':['00100','01100','00100','00100','00100','00100','01110'],'2':['01110','10001','00001','00010','00100','01000','11111'],'3':['11110','00001','00001','01110','00001','00001','11110'],'4':['00010','00110','01010','10010','11111','00010','00010'],'5':['11111','10000','10000','11110','00001','00001','11110'],'6':['01110','10000','10000','11110','10001','10001','01110'],'7':['11111','00001','00010','00100','01000','01000','01000'],'8':['01110','10001','10001','01110','10001','10001','01110'],'9':['01110','10001','10001','01111','00001','00001','01110'],'-':['00000','00000','00000','11111','00000','00000','00000'],'&':['01100','10010','10100','01000','10101','10010','01101']
};
const matrix=$('#game-matrix'),matrixContext=matrix.getContext('2d');
let matrixStarted=performance.now(),lastMatrixFrame=-100;
function drawMatrix(now){
 if(matrixContext&&now-lastMatrixFrame>=50){
  lastMatrixFrame=now;const ctx=matrixContext,cols=128,rows=20,step=6;ctx.clearRect(0,0,matrix.width,matrix.height);
  function dot(x,y,on,color){ctx.fillStyle=on?color:'#22150f';ctx.beginPath();ctx.arc(x*step+3,y*step+3,on?2.1:1.6,0,Math.PI*2);ctx.fill();}
  const bits=Array.from({length:rows},()=>Array(cols).fill(false));
  function text(value,y,x){for(const letter of value){const g=glyphs[letter]||glyphs[' '];for(let r=0;r<7;r++)for(let c=0;c<5;c++){const col=x+c;if(col>=0&&col<cols&&g[r][c]==='1')bits[y+r][col]=true;}x+=6;}}
  const title=roundMode==='finished'?(score>=50?'ORDER UP - GUSHERS EARNED':'DISH SERVED - TRY AGAIN'):chosenDish().name;
  const width=title.length*6,shift=width>cols&&!reduceMotion?Math.floor(Math.max(0,now-matrixStarted-1300)/65)%(width+28):0;
  text(title,1,width<=cols?Math.floor((cols-width)/2):2-shift);
  if(shift>0)text(title,1,2-shift+width+28);
  const bottom=`SCORE ${String(Math.floor(score)).padStart(3,'0')} TIME ${String(seconds).padStart(2,'0')}`;
  text(bottom,11,Math.floor((cols-bottom.length*6)/2));
  for(let y=0;y<rows;y++)for(let x=0;x<cols;x++)dot(x,y,bits[y][x],y<9?'#ff663a':seconds<=10&&roundMode==='cooking'?'#ff4d3a':'#e6f27c');
 }
 requestAnimationFrame(drawMatrix);
}
renderIngredients();renderHeat();renderRound();requestAnimationFrame(drawMatrix);
setInterval(()=>advanceRound(performance.now()),100);

// Opt-in Web Audio illustration. Sound is synthesized in the browser, independently
// of the PIC32 waveform generator. No audio plays before a visitor presses the button.
class PanAudio {
 constructor() {
  const Context = window.AudioContext || window.webkitAudioContext;
  if (!Context) throw new Error('Web Audio unavailable');
  this.ctx = new Context(); this.zone = currentFood && roundMode !== 'finished' ? zone : 'silent'; this.step = 0; this.nextNote = 0; this.lastPop = 0;
  this.master = this.ctx.createGain(); this.master.gain.value = .24; this.master.connect(this.ctx.destination);
  this.noiseGain = this.ctx.createGain(); this.noiseGain.gain.value = 0;
  const buffer = this.ctx.createBuffer(1, this.ctx.sampleRate * 2, this.ctx.sampleRate);
  const data = buffer.getChannelData(0); for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  this.buffer = buffer; this.bed = this.ctx.createBufferSource(); this.bed.buffer = buffer; this.bed.loop = true;
  const highpass = this.ctx.createBiquadFilter(); highpass.type = 'highpass'; highpass.frequency.value = 1400;
  const lowpass = this.ctx.createBiquadFilter(); lowpass.type = 'lowpass'; lowpass.frequency.value = 6500;
  this.bed.connect(highpass); highpass.connect(lowpass); lowpass.connect(this.noiseGain); this.noiseGain.connect(this.master); this.bed.start();
 }
 async start() { await this.ctx.resume(); this.nextNote = this.ctx.currentTime + .08; this.setZone(this.zone); if(this.zone!=='silent')this.chime(this.zone); this.timer = setInterval(() => this.tick(), 60); }
 setZone(value) { this.zone = value; this.noiseGain.gain.setTargetAtTime({ silent:0, low: .008, right: .022, high: .045 }[value], this.ctx.currentTime, .12); }
 note(hz, start, duration, gain = .14, type = 'triangle') {
  const osc = this.ctx.createOscillator(); const env = this.ctx.createGain(); osc.type = type; osc.frequency.value = hz;
  env.gain.setValueAtTime(0, start); env.gain.linearRampToValueAtTime(gain, start + .012); env.gain.exponentialRampToValueAtTime(.0001, start + duration);
  osc.connect(env); env.connect(this.master); osc.start(start); osc.stop(start + duration + .015);
 }
 chime(value) { const pitches = { low: [523.25, 392], right: [523.25, 659.25, 783.99], high: [880, 1046.5, 880] }[value]; pitches.forEach((hz, i) => this.note(hz, this.ctx.currentTime + i * .13, .55, .18, 'sine')); }
 tick() {
  const now = this.ctx.currentTime;
  const sequence = [64, 0, 67, 71, 69, 67, 64, 62, 60, 0, 64, 67, 69, 0, 67, 62, 64, 67, 72, 71, 69, 67, 64, 0, 62, 64, 67, 69, 67, 64, 62, 0];
  while (this.nextNote < now + .15) {
   const midi = sequence[this.step % sequence.length]; if (midi) this.note(440 * 2 ** ((midi - 69) / 12), this.nextNote, .24, .12);
   if (this.step % 4 === 0) this.note([130.81, 110, 87.31, 98][Math.floor(this.step / 8) % 4], this.nextNote, .4, .15, 'sine');
   this.step++; this.nextNote += .234375;
  }
  const chance = { silent:0, low: .05, right: .16, high: .38 }[this.zone];
  if (Math.random() < chance && now - this.lastPop > .04) {
   this.lastPop = now; const pop = this.ctx.createBufferSource(); const env = this.ctx.createGain(); const filter = this.ctx.createBiquadFilter();
   pop.buffer = this.buffer; filter.type = 'bandpass'; filter.frequency.value = 1500 + Math.random() * 4500; filter.Q.value = .8;
   env.gain.setValueAtTime(0, now); env.gain.linearRampToValueAtTime(this.zone === 'high' ? .18 : .09, now + .002); env.gain.exponentialRampToValueAtTime(.0001, now + .07);
   pop.connect(filter); filter.connect(env); env.connect(this.master); pop.start(now, Math.random()); pop.stop(now + .08);
  }
 }
 async stop() { clearInterval(this.timer); if (this.ctx.state !== 'closed') { this.master.gain.setTargetAtTime(0, this.ctx.currentTime, .02); await new Promise(resolve => setTimeout(resolve, 100)); await this.ctx.close(); } }
}
const soundButton = $('#sound-toggle'); let audioBusy = false;
async function stopAudio() { if (!audio) return; const old = audio; audio = null; soundButton.setAttribute('aria-pressed', 'false'); soundButton.innerHTML = '<span aria-hidden="true">♫</span> Sound off'; await old.stop(); }
soundButton.addEventListener('click', async () => {
 if (audioBusy) return; audioBusy = true;
 try { if (audio) await stopAudio(); else { audio = new PanAudio(); await audio.start(); soundButton.setAttribute('aria-pressed', 'true'); soundButton.innerHTML = '<span aria-hidden="true">♫</span> Sound on'; } }
 catch (error) { if (audio) { await audio.stop().catch(() => {}); audio = null; } soundButton.textContent = 'Sound unavailable'; soundButton.setAttribute('aria-pressed', 'false'); console.warn('Browser audio could not start.', error); }
 finally { audioBusy = false; }
});
document.addEventListener('visibilitychange', () => { if (document.hidden && audio) stopAudio(); });

function setupTabs(container, callback) {
 const tabs = $$('[role=tab]', container);
 tabs.forEach((tab, index) => {
  tab.addEventListener('click', () => { tabs.forEach(t => { const selected = t === tab; t.setAttribute('aria-selected', String(selected)); t.tabIndex = selected ? 0 : -1; }); callback(tab); });
  tab.addEventListener('keydown', event => { let next; if (['ArrowRight', 'ArrowDown'].includes(event.key)) next = (index + 1) % tabs.length; if (['ArrowLeft', 'ArrowUp'].includes(event.key)) next = (index - 1 + tabs.length) % tabs.length; if (event.key === 'Home') next = 0; if (event.key === 'End') next = tabs.length - 1; if (next !== undefined) { event.preventDefault(); tabs[next].focus(); tabs[next].click(); } });
 });
}
setupTabs($('.brain-tabs'), tab => { for (const panel of $$('.brain-panel')) panel.hidden = panel.id !== tab.getAttribute('aria-controls'); });
const services = {
 game: ['Run the round.', 'GameSM owns Welcome, dish introduction, 60-second cooking, Finish eligibility at 30 seconds, and the result/reset sequence. Inactivity returns to Welcome after 20 seconds without reward.', ['Welcome', 'Dish intro', 'Cooking', 'Result']],
 ingredient: ['Add it once.', 'IngredientSM accepts Protein, Carb, Vegetables, Spice and Oil once per round. ON adds an ingredient; OFF is activity only. A switch already ON at cooking start must first go OFF. PIC2 sends reliable edges; PIC1 owns acceptance.', ['Ready', 'Accepted mask', 'Round reset']],
 heat: ['Keep it cooking.', 'CookSM uses the newest ingredient target with a ±4 cm band. Heat earns up to +1 point/s. Three continuous seconds TOO MUCH burns it for −10 once and stops heat points. Targets are 14/24/32/28/10 cm.', ['Waiting', 'Not enough', 'Just right', 'Too much / burnt']],
 stir: ['Turn input into motion.', 'PIC1 reads joystick X/Y and sends 50 Hz pulses to the spoon servo. Checked-off scores stirring time; the latest branch detects full eight-sector joystick circles and awards +1 up to each ideal count, then −1. The reward uses a separate MG995 servo.', ['Learn center', 'Follow joystick', 'Return home']],
 score: ['One owner for the score.', 'ScoreSM alone accumulates signed millipoints, clamps the score at zero, and finalizes after cooking updates. Correct/wrong ingredients give +10/−10; heat, burn, stirring and flips also contribute. Finish/time-limit endings dispense only at score ≥50.', ['Idle', 'Active', 'Finalized']],
 pan: ['Put the food in the pan.', 'ButtonEInk accumulates accepted ingredient icons, batches additions after a 500 ms pause, and services a non-blocking black/white refresh. Clear frames finish before new-round icons. Matrices separately show the dish, live time/score and end message.', ['White', 'Batch icons', 'Refresh', 'Clear']]
};
setupTabs($('.service-menu'), tab => {
 const [title, text, states] = services[tab.dataset.service]; const panel = $('#service-panel'); panel.setAttribute('aria-labelledby', tab.id); panel.replaceChildren();
 const label = document.createElement('p'); label.className = 'micro-label'; label.textContent = 'INTEGRATED SOURCE + REVISION NOTES';
 const h3 = document.createElement('h3'); h3.textContent = title; const p = document.createElement('p'); p.textContent = text; const flow = document.createElement('div'); flow.className = 'state-flow';
 states.forEach((state, i) => { if (i) { const dot = document.createElement('i'); dot.textContent = '·'; dot.setAttribute('aria-hidden', 'true'); flow.append(dot); } const chip = document.createElement('span'); chip.textContent = state; flow.append(chip); }); panel.append(label, h3, p, flow);
});

// Source viewer uses textContent so C comments are always displayed as text.
let sourceFiles = []; let activeSource = ''; let sourceController;
async function openSource(file) {
 if (sourceController) sourceController.abort(); sourceController = new AbortController();
 activeSource = file.path; $('#source-current').textContent = file.name; $('#source-raw').href = file.path; $('#source-code').textContent = 'Loading source…'; $('#source-status').textContent = 'Loading ' + file.name;
 $$('#source-files button').forEach(b => b.setAttribute('aria-current', String(b.dataset.path === file.path)));
 try { let code = file.content; if (code === undefined) { const response = await fetch(file.path, { signal: sourceController.signal }); if (!response.ok) throw new Error('File unavailable'); code = await response.text(); } $('#source-code').textContent = code; $('#source-status').textContent = file.name + ' loaded'; }
 catch (error) { if (error.name !== 'AbortError') { $('#source-code').textContent = 'This file could not load. Open the raw file or download the full source appendix below.'; $('#source-status').textContent = 'Source could not load'; } }
}
function filterSources() {
 const group = $('#source-group').value; const query = $('#source-search').value.trim().toLowerCase(); const matches = sourceFiles.filter(f => f.group === group && f.label.toLowerCase().includes(query)); const list = $('#source-files'); list.replaceChildren();
 if (!matches.length) { const p = document.createElement('p'); p.textContent = 'No matching files.'; list.append(p); return; }
 matches.forEach(file => { const b = document.createElement('button'); b.textContent = file.name; b.title = file.label; b.dataset.path = file.path; b.setAttribute('aria-current', String(file.path === activeSource)); b.addEventListener('click', () => openSource(file)); list.append(b); });
}
async function initSource() {
 try { const bundled = document.querySelector('#bundled-sources'); if (bundled) sourceFiles = JSON.parse(bundled.textContent); else { const response = await fetch('source-index.json'); if (!response.ok) throw new Error('No source index'); sourceFiles = await response.json(); } filterSources(); const first = sourceFiles.find(f => f.name === 'GameSM.c' && f.group === 'integrated'); if (first) openSource(first); }
 catch { $('#source-files').textContent = 'Use a web server or GitHub Pages for source browsing. The full PDF appendix is always linked above.'; }
}
$('#source-group').addEventListener('change', filterSources); $('#source-search').addEventListener('input', filterSources); initSource();
$$('[data-bom-filter]').forEach(button => button.addEventListener('click', () => { $$('[data-bom-filter]').forEach(b => b.setAttribute('aria-pressed', String(b === button))); const category = button.dataset.bomFilter; $$('#bom-register tr').forEach(row => { row.hidden = category !== 'all' && !row.dataset.categories.split(' ').includes(category); }); }));
const lightbox = $('#lightbox');
$$('[data-lightbox]').forEach(button => button.addEventListener('click', () => { $('img', lightbox).src = button.dataset.lightbox; $('img', lightbox).alt = button.dataset.caption; $('p', lightbox).textContent = button.dataset.caption; lightbox.showModal(); }));
$('.lightbox-close').addEventListener('click', () => lightbox.close());
lightbox.addEventListener('click', event => { if (event.target === lightbox) { const r = lightbox.getBoundingClientRect(); if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) lightbox.close(); } });
