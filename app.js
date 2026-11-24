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

// A browser explainer, not a port of the final game or its scoring rules.
const distance = $('#distance');
let zone = 'right';
let audio = null;
function renderHeat() {
 const mm = Number(distance.value);
 const next = mm < 150 ? 'high' : mm > 280 ? 'low' : 'right';
 const config = { high: ['TOO MUCH', '#ff8150', '♨'], low: ['NOT ENOUGH', '#83cafa', '❄'], right: ['JUST RIGHT', '#9fdfbf', '✳'] }[next];
 const stage = $('.demo-stage');
 stage.style.setProperty('--heat', config[1]); stage.style.setProperty('--heat-level', `${(400 - mm) / 350 * 100}%`);
 $('#heat-zone').textContent = config[0]; $('#heat-icon').textContent = config[2]; $('#distance-output').textContent = String(mm);
 distance.setAttribute('aria-valuetext', `${mm} millimeters, ${config[0].toLowerCase()}`);
 if (audio) { audio.setZone(next); if (next !== zone) audio.chime(next); }
 zone = next;
}
distance.addEventListener('input', renderHeat); renderHeat();
const ingredients = new Set();
function renderIngredients() {
 const screen = $('#pan-ingredients'); screen.replaceChildren();
 if (!ingredients.size) { const empty = document.createElement('span'); empty.className = 'empty-pan'; empty.textContent = 'ADD SOMETHING GOOD'; screen.append(empty); }
 else for (const ingredient of ingredients) { const item = document.createElement('span'); item.className = 'food'; item.textContent = ingredient; screen.append(item); }
}
$$('[data-ingredient]').forEach(button => button.addEventListener('click', () => { const value = button.dataset.ingredient; if (ingredients.has(value)) ingredients.delete(value); else ingredients.add(value); button.setAttribute('aria-pressed', String(ingredients.has(value))); renderIngredients(); }));
const joystick = $('#joystick');
const knob = $('.joystick-knob');
let sweep = 0;
function setSweep(value, y = 0) { sweep = Math.max(-55, Math.min(55, value)); $('#pan-scene').style.setProperty('--sweep', `${sweep}deg`); knob.style.transform = `translate(${sweep / 55 * 23}px,${Math.max(-18, Math.min(18, y))}px)`; joystick.setAttribute('aria-valuenow', String(Math.round(sweep))); joystick.setAttribute('aria-valuetext', `${Math.round(sweep)} degrees of spatula sweep`); }
function drag(event) { const bounds = joystick.getBoundingClientRect(); setSweep((event.clientX - bounds.left - bounds.width / 2) / (bounds.width / 2) * 55, (event.clientY - bounds.top - bounds.height / 2) * .5); }
joystick.addEventListener('pointerdown', event => { event.preventDefault(); joystick.focus({ preventScroll: true }); joystick.setPointerCapture(event.pointerId); drag(event); });
joystick.addEventListener('pointermove', event => { if (joystick.hasPointerCapture(event.pointerId)) drag(event); });
for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) joystick.addEventListener(type, () => setSweep(0));
joystick.addEventListener('keydown', event => { const commands = { ArrowLeft: -10, ArrowDown: -10, ArrowRight: 10, ArrowUp: 10 }; if (event.key in commands) { event.preventDefault(); setSweep(sweep + commands[event.key]); } else if (event.key === 'Home' || event.key === 'Escape') { event.preventDefault(); setSweep(0); } });
$('#reset-demo').addEventListener('click', () => { distance.value = '220'; renderHeat(); ingredients.clear(); $$('[data-ingredient]').forEach(b => b.setAttribute('aria-pressed', 'false')); renderIngredients(); setSweep(0); });

// Opt-in Web Audio illustration. Sound is synthesized in the browser, independently
// of the PIC32 waveform generator. No audio plays before a visitor presses the button.
class PanAudio {
 constructor() {
  const Context = window.AudioContext || window.webkitAudioContext;
  if (!Context) throw new Error('Web Audio unavailable');
  this.ctx = new Context(); this.zone = zone; this.step = 0; this.nextNote = 0; this.lastPop = 0;
  this.master = this.ctx.createGain(); this.master.gain.value = .24; this.master.connect(this.ctx.destination);
  this.noiseGain = this.ctx.createGain(); this.noiseGain.gain.value = 0;
  const buffer = this.ctx.createBuffer(1, this.ctx.sampleRate * 2, this.ctx.sampleRate);
  const data = buffer.getChannelData(0); for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  this.buffer = buffer; this.bed = this.ctx.createBufferSource(); this.bed.buffer = buffer; this.bed.loop = true;
  const highpass = this.ctx.createBiquadFilter(); highpass.type = 'highpass'; highpass.frequency.value = 1400;
  const lowpass = this.ctx.createBiquadFilter(); lowpass.type = 'lowpass'; lowpass.frequency.value = 6500;
  this.bed.connect(highpass); highpass.connect(lowpass); lowpass.connect(this.noiseGain); this.noiseGain.connect(this.master); this.bed.start();
 }
 async start() { await this.ctx.resume(); this.nextNote = this.ctx.currentTime + .08; this.setZone(this.zone); this.chime(this.zone); this.timer = setInterval(() => this.tick(), 60); }
 setZone(value) { this.zone = value; this.noiseGain.gain.setTargetAtTime({ low: .008, right: .022, high: .045 }[value], this.ctx.currentTime, .12); }
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
  const chance = { low: .05, right: .16, high: .38 }[this.zone];
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
 game: ['Run the round.', 'Own the game clock, inactivity timer, Finish eligibility, result/reset sequence and reward request. The final hardware instead has explicit green Start and red Stop controls.', ['Welcome', 'Cooking', 'Result / timeout']],
 ingredient: ['Add it once.', 'The original design records an added-ingredient mask, suppresses repeated additions, and posts accepted ingredients to the interested services. Final ingredient names and switch rules await the grading code.', ['Ready', 'Locked']],
 heat: ['Keep it cooking.', 'The checkpoint proposes continuous heat scoring and a burn after three seconds above 1.5 times an ingredient target. The separate peripheral code provides distance-based zones, LCD feedback and sizzle.', ['PanEmpty', 'Cooking', 'Overheating', 'Burning']],
 stir: ['Turn input into motion.', 'The final machine uses a small servo to sweep the spatula back and forth under joystick control. The older FSM instead planned angular movement tracking and per-ingredient stir time; final scoring needs code verification.', ['Idle', 'NotStirring', 'Stirring']],
 score: ['One owner for the score.', 'The planned Score service accumulates ingredient, heat and stirring points and applies burn penalties. Recipe membership and weights are unconfigured in the supplied scaffold.', ['Idle', 'Active']],
 pan: ['Put the food in the pan.', 'The final e-paper screen displays an ingredient icon when its switch is turned on. The original Pan Display service planned a pan-shaped matrix with icons and swirl animation; that concept changed.', ['Attract', 'Ingredient / pan view', 'End indication']]
};
setupTabs($('.service-menu'), tab => {
 const [title, text, states] = services[tab.dataset.service]; const panel = $('#service-panel'); panel.setAttribute('aria-labelledby', tab.id); panel.replaceChildren();
 const label = document.createElement('p'); label.className = 'micro-label'; label.textContent = 'ORIGINAL CHECKPOINT DESIGN + FINAL BUILD NOTES';
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
 try { const bundled = document.querySelector('#bundled-sources'); if (bundled) sourceFiles = JSON.parse(bundled.textContent); else { const response = await fetch('source-index.json'); if (!response.ok) throw new Error('No source index'); sourceFiles = await response.json(); } filterSources(); const first = sourceFiles.find(f => f.name === 'heat_audio.c' && f.group === 'peripheral'); if (first) openSource(first); }
 catch { $('#source-files').textContent = 'Use a web server or GitHub Pages for source browsing. The full PDF appendix is always linked above.'; }
}
$('#source-group').addEventListener('change', filterSources); $('#source-search').addEventListener('input', filterSources); initSource();
$$('[data-bom-filter]').forEach(button => button.addEventListener('click', () => { $$('[data-bom-filter]').forEach(b => b.setAttribute('aria-pressed', String(b === button))); const category = button.dataset.bomFilter; $$('#bom-register tr').forEach(row => { row.hidden = category !== 'all' && !row.dataset.categories.split(' ').includes(category); }); }));
const lightbox = $('#lightbox');
$$('[data-lightbox]').forEach(button => button.addEventListener('click', () => { $('img', lightbox).src = button.dataset.lightbox; $('img', lightbox).alt = button.dataset.caption; $('p', lightbox).textContent = button.dataset.caption; lightbox.showModal(); }));
$('.lightbox-close').addEventListener('click', () => lightbox.close());
lightbox.addEventListener('click', event => { if (event.target === lightbox) { const r = lightbox.getBoundingClientRect(); if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) lightbox.close(); } });
