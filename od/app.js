"use strict";
/* ===========================================================================
   SiliFlow · Apple HIG redesign — interactive prototype
   取材自 SiliFlow-2026 源码：wxAUI 工作台（元件库 / 多标签画布 / 属性 + 层次树 /
   波形 + 消息），内置元件库 25 项，组合逻辑实时仿真（S1）。
   =========================================================================== */
const $  = (s, r=document) => r.querySelector(s);
const $$ = (s, r=document) => Array.from(r.querySelectorAll(s));
const clamp = (v,a,b)=>Math.max(a,Math.min(b,v));
let seq = 1; const uid = ()=>seq++;

/* ------------------------------------------------------------------ theme */
const THEME = {
  light:{ bg:'#fbfbfc', grid:'#e8e8ea', gmajor:'#d7d7db', stroke:'#33373d', fill:'#ffffff', dim:'#78808a',
    text:'#1d1d1f', pin:'#5f6b7a', pinHot:'#007aff', sel:'#007aff', hover:'#66bb6a',
    s0:'#8a8a90', s1:'#34c759', sx:'#ff3b30', sz:'#c9c9ce', idle:'#2e7d5b', ledOff:'#f0f0f2',
    selFill:'rgba(0,122,255,.10)' },
  dark:{ bg:'#1b1b1d', grid:'#26262a', gmajor:'#313138', stroke:'#dcdce0', fill:'#2c2c2e', dim:'#9a9aa0',
    text:'#f5f5f7', pin:'#8b93a0', pinHot:'#0a84ff', sel:'#0a84ff', hover:'#4f9e78',
    s0:'#98989d', s1:'#30d158', sx:'#ff453a', sz:'#48484a', idle:'#4f9e78', ledOff:'#3a3a3c',
    selFill:'rgba(10,132,255,.12)' }
};
let theme = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
const T = () => THEME[theme];
const sigColor = s => { const t=T(); return s==='0'?t.s0 : s==='1'?t.s1 : s==='X'?t.sx : t.sz; };

/* ------------------------------------------------------------ component lib */
const LIB_CATS = ['逻辑门','输入输出','常用器件','布线'];
const DEF = {};
const addDef = d => { DEF[d.type]=d; };
const gatePins = (a3,inv) => { const p=[{n:'A',d:'in',x:0,y:a3?10:15},{n:'B',d:'in',x:0,y:a3?30:45}];
  if(a3) p.push({n:'C',d:'in',x:0,y:50}); p.push({n:'Y',d:'out',x:inv?88:80,y:30}); return p; };
const mkGate=(type,name,gate,shape,inv,a3)=>{ const d={type,name,cat:'逻辑门',kind:'gate',gate,shape,inv,a3,w:inv?88:80,h:60};
  d.pins=gatePins(a3,inv); addDef(d); };
mkGate('and','与门','and','and',false,false);
mkGate('and3','三输入与门','and','and',false,true);
mkGate('nand','与非门','nand','and',true,false);
mkGate('nand3','三输入与非门','nand','and',true,true);
mkGate('or','或门','or','or',false,false);
mkGate('or3','三输入或门','or','or',false,true);
mkGate('nor','或非门','nor','or',true,false);
mkGate('nor3','三输入或非门','nor','or',true,true);
mkGate('xor','异或门','xor','xor',false,false);
mkGate('xor3','三输入异或门','xor','xor',false,true);
mkGate('xnor','同或门','xnor','xor',true,false);
addDef({type:'not',name:'非门',cat:'逻辑门',kind:'gate',gate:'not',shape:'buf',inv:true,w:62,h:50,pins:[{n:'A',d:'in',x:0,y:25},{n:'Y',d:'out',x:60,y:25}]});
addDef({type:'buf',name:'缓冲器',cat:'逻辑门',kind:'gate',gate:'buf',shape:'buf',inv:false,w:52,h:50,pins:[{n:'A',d:'in',x:0,y:25},{n:'Y',d:'out',x:50,y:25}]});
addDef({type:'input',name:'输入引脚',cat:'输入输出',kind:'port',port:'in',w:40,h:30,pins:[{n:'Y',d:'out',x:40,y:15}]});
addDef({type:'output',name:'输出引脚',cat:'输入输出',kind:'port',port:'out',w:40,h:30,pins:[{n:'A',d:'in',x:0,y:15}]});
addDef({type:'const0',name:'常量 0',cat:'输入输出',kind:'const',val:'0',w:40,h:24,pins:[{n:'Y',d:'out',x:40,y:12}]});
addDef({type:'const1',name:'常量 1',cat:'输入输出',kind:'const',val:'1',w:40,h:24,pins:[{n:'Y',d:'out',x:40,y:12}]});
addDef({type:'ground',name:'接地',cat:'输入输出',kind:'const',val:'0',w:30,h:30,pins:[{n:'Y',d:'out',x:15,y:0}]});
addDef({type:'led',name:'指示灯',cat:'输入输出',kind:'led',w:30,h:40,pins:[{n:'A',d:'in',x:15,y:40}]});
addDef({type:'clock',name:'时钟源',cat:'输入输出',kind:'clock',w:60,h:40,pins:[{n:'Y',d:'out',x:60,y:20}]});
addDef({type:'mux2',name:'二选一多路器',cat:'常用器件',kind:'gate',gate:'mux2',shape:'mux',w:60,h:80,pins:[{n:'A',d:'in',x:0,y:15},{n:'B',d:'in',x:0,y:65},{n:'S',d:'in',x:20,y:80},{n:'Y',d:'out',x:50,y:40}]});
addDef({type:'dff',name:'D 触发器',cat:'常用器件',kind:'dff',w:70,h:80,pins:[{n:'D',d:'in',x:0,y:20},{n:'CLK',d:'in',x:0,y:63},{n:'Q',d:'out',x:70,y:20},{n:'QN',d:'out',x:70,y:63}]});
addDef({type:'addr_full',name:'全加器',cat:'常用器件',kind:'gate',gate:'add',shape:'box',w:70,h:80,pins:[{n:'A',d:'in',x:0,y:15},{n:'B',d:'in',x:0,y:40},{n:'CIN',d:'in',x:0,y:65},{n:'S',d:'out',x:70,y:25},{n:'COUT',d:'out',x:70,y:55}]});
addDef({type:'splitter',name:'位分离器',cat:'布线',kind:'splitter',w:50,h:60,pins:[{n:'IN',d:'in',x:0,y:30},{n:'Y0',d:'out',x:50,y:15},{n:'Y1',d:'out',x:50,y:45}]});
addDef({type:'tunnel',name:'命名隧道',cat:'布线',kind:'tunnel',w:60,h:30,pins:[{n:'P',d:'inout',x:0,y:15}]});
const LIB_TOTAL = Object.keys(DEF).length;

/* ------------------------------------------------------------ circuit model */
const newSheet = (name, comps=[], wires=[], notes=[]) => ({ name, comps, wires, notes, sel:new Set() });
const mkComp = (type,x,y,label='',val) => { const d=DEF[type];
  return { id:uid(), type, x, y, rot:0, mirror:false, label, val:(val!==undefined?val:(d.val||'')),
    inState: type==='input' ? (label==='' ? '1' : '1') : null }; };
const mkWire = (ac,ap,bc,bp) => ({ id:uid(), a:{c:ac,p:ap}, b:{c:bc,p:bp}, signal:'Z', sel:false });

const sheets = [ newSheet('main'), newSheet('sheet2') ];
(function seed(){
  const s=sheets[0];
  const A=mkComp('input',80,120,'A','1'), B=mkComp('input',80,260,'B','1');
  const AND=mkComp('and',300,150), Y=mkComp('output',560,165,'Y'), LED=mkComp('led',560,300);
  s.comps.push(A,B,AND,Y,LED);
  s.wires.push( mkWire(A.id,0,AND.id,0), mkWire(B.id,0,AND.id,1), mkWire(AND.id,2,Y.id,0), mkWire(AND.id,2,LED.id,0) );
})();
let activeSheet='main';
const proj={ name:'未命名', dirty:false };
const sheet = () => sheets.find(s=>s.name===activeSheet);
const compById = id => sheet().comps.find(c=>c.id===id);
const defOf = c => DEF[c.type];

/* ------------------------------------------------------------ app state */
const view = { ox:40, oy:30, scale:1 };
const st = { tool:'Select', draw:'rect', placing:null, simRunning:false, simHz:2, tick:0,
  selectedKind:null, selectedId:null, hoverPin:null, drag:null, wireStart:null, mouse:null, noSnap:false };
const opts = { showGrid:true, showPinNames:true, showLabels:true };

/* ------------------------------------------------------------ geometry */
function rot90(dx,dy,r){ r=((r%360)+360)%360; if(r===90)return[-dy,dx]; if(r===180)return[-dx,-dy]; if(r===270)return[dy,-dx]; return[dx,dy]; }
function localToWorld(c,lx,ly){ const d=defOf(c), cx=d.w/2, cy=d.h/2;
  let dx=lx-cx, dy=ly-cy; if(c.mirror) dx=-dx; const r=rot90(dx,dy,c.rot);
  return { x:c.x+cx+r[0], y:c.y+cy+r[1] }; }
function worldToLocal(c,wx,wy){ const d=defOf(c), cx=d.w/2, cy=d.h/2;
  let dx=wx-(c.x+cx), dy=wy-(c.y+cy); const r=rot90(dx,dy,-c.rot); let lx=r[0]; if(c.mirror) lx=-lx;
  return { x:lx+cx, y:r[1]+cy }; }
const pinWorld = (c,i) => { const p=defOf(c).pins[i]; return localToWorld(c,p.x,p.y); };
function compBounds(c){ const d=defOf(c);
  const pts=[[0,0],[d.w,0],[d.w,d.h],[0,d.h]].map(([x,y])=>localToWorld(c,x,y));
  const xs=pts.map(p=>p.x), ys=pts.map(p=>p.y);
  return { x:Math.min(...xs), y:Math.min(...ys), w:Math.max(...xs)-Math.min(...xs), h:Math.max(...ys)-Math.min(...ys) }; }
const wireEnd = e => { const c=compById(e.c); return c?pinWorld(c,e.p):null; };
function wirePath(w){ const a=wireEnd(w.a), b=wireEnd(w.b); if(!a||!b) return null;
  if(Math.round(a.y)===Math.round(b.y)) return [a,b];
  const mx=(a.x+b.x)/2; return [a,{x:mx,y:a.y},{x:mx,y:b.y},b]; }
function refreshWirePaths(){ sheet().wires.forEach(w=>w.path=wirePath(w)); }

/* ------------------------------------------------------------ connectivity + sim */
let NET = { netOf:new Map(), nets:[], value:[] };
const key = (c,p) => c+':'+p;
function buildNets(){
  const parent=new Map();
  const find=k=>{ if(!parent.has(k))parent.set(k,k); let r=k; while(parent.get(r)!==r) r=parent.get(r);
    while(parent.get(k)!==r){ const n=parent.get(k); parent.set(k,r); k=n; } return r; };
  const uni=(a,b)=>{ const ra=find(a), rb=find(b); if(ra!==rb) parent.set(ra,rb); };
  sheet().wires.forEach(w=>{ if(w.a.c!=null&&w.b.c!=null){ const a=key(w.a.c,w.a.p), b=key(w.b.c,w.b.p); if(find(a)!==find(b)) uni(a,b); } });
  const tByLabel={};
  sheet().comps.forEach(c=>{ if(c.type==='tunnel'){ const k=key(c.id,0); if(!parent.has(k))parent.set(k,k);
    const lb=(c.label||'').trim()||('tunnel_'+c.id);
    if(tByLabel[lb]){ const r1=find(k), r2=find(tByLabel[lb]); if(r1!==r2) parent.set(r1,r2); } tByLabel[lb]=k; } });
  const roots=new Map(), netOf=new Map(), nets=[];
  [...parent.keys()].forEach(k=>{ const r=find(k); if(!roots.has(r)){ roots.set(r,nets.length); nets.push({pins:[],wires:[]}); } netOf.set(k,roots.get(r)); });
  sheet().wires.forEach(w=>{ if(w.a.c!=null){ const k=key(w.a.c,w.a.p); if(netOf.has(k)) nets[netOf.get(k)].wires.push(w.id); }
    if(w.b.c!=null){ const k=key(w.b.c,w.b.p); if(netOf.has(k)) nets[netOf.get(k)].wires.push(w.id); } });
  sheet().comps.forEach(c=>defOf(c).pins.forEach((p,i)=>{ const k=key(c.id,i); if(netOf.has(k)) nets[netOf.get(k)].pins.push({c:c.id,i}); }));
  nets.forEach((n,idx)=>{ let nm='net'+(idx+1); for(const pr of n.pins){ const c=compById(pr.c);
    if(c&&c.label&&(c.type==='input'||c.type==='output'||c.type==='tunnel')){ nm=c.label; break; } } n.name=nm; });
  NET={ netOf, nets, value:new Array(nets.length).fill('Z') };
}
function combine(vals){ const a=vals.filter(v=>v!=='Z'); if(!a.length) return 'Z';
  if(a.includes('X')) return 'X'; const h0=a.includes('0'), h1=a.includes('1'); if(h0&&h1) return 'X'; return h0?'0':'1'; }
function gateEval(shape,inv,ins){
  const hasX=ins.includes('X'), hasZ=ins.includes('Z');
  let r;
  if(shape==='and') r = ins.includes('0')?'0' : (hasX||hasZ)?'X':'1';
  else if(shape==='or') r = ins.includes('1')?'1' : (hasX||hasZ)?'X':'0';
  else if(shape==='xor') r = (hasX||hasZ)?'X' : (ins.filter(v=>v==='1').length%2?'1':'0');
  else if(shape==='buf'){ r=ins[0]; if(r==='Z'||r==null) r='X'; }
  else if(shape==='mux'){ const s=ins[2]; r = s==='0'?ins[0] : s==='1'?ins[1] : 'X'; }
  else if(shape==='box'){ r = (ins[0]==='1'?1:0)^(ins[1]==='1'?1:0)^(ins[2]==='1'?1:0) ? '1':'0'; }
  else r='X';
  if(inv) r = r==='0'?'1' : r==='1'?'0' : 'X';
  return r;
}
const readNet = (c,i,val) => { const net=NET.netOf.get(key(c,i)); return net===undefined?'Z':val[net]; };
function evaluate(){
  buildNets(); const n=NET.nets.length; let val=new Array(n).fill('Z');
  for(let it=0; it<80; it++){
    const contrib = Array.from({length:n},()=>[]);
    for(const c of sheet().comps){
      const d=DEF[c.type]; let out=null;
      if(d.kind==='port') out = c.type==='input' ? (c.inState==='1'?'1':'0') : null;
      else if(d.kind==='const') out=d.val;
      else if(d.kind==='clock') out = st.simRunning ? (st.tick%2?'1':'0') : '0';
      else if(d.kind==='led') out=null;
      else if(d.kind==='tunnel') out=readNet(c.id,0,val);
      else if(d.kind==='splitter') out=readNet(c.id,0,val);
      else if(d.kind==='dff') out=readNet(c.id,0,val);
      else if(d.kind==='gate'){ const ins=[]; d.pins.forEach((p,i)=>{ if(p.d==='in') ins.push(readNet(c.id,i,val)); }); out=gateEval(d.shape,d.inv,ins); }
      if(out!=null) d.pins.forEach((p,i)=>{ if(p.d==='out'||p.d==='inout'){ const net=NET.netOf.get(key(c.id,i)); if(net!==undefined) contrib[net].push(out); } });
    }
    const next=contrib.map(combine);
    let changed=false; for(let i=0;i<n;i++){ if(next[i]!==val[i]){ changed=true; break; } }
    val=next; if(!changed) break;
  }
  NET.value=val;
  sheet().wires.forEach(w=>{ const k=key(w.a.c,w.a.p); w.signal = NET.netOf.has(k)? val[NET.netOf.get(k)] : 'Z'; });
}
function countConflicts(){ let n=0; NET.nets.forEach((net,i)=>{ const dr=[];
  net.pins.forEach(pr=>{ const c=compById(pr.c); if(c&&defOf(c).pins[pr.i].d==='out') dr.push(NET.value[i]); });
  const a=dr.filter(v=>v!=='Z'); if(a.includes('0')&&a.includes('1')) n++; }); return n; }
function simStatus(){ const s=sheet(); if(!s.comps.length&&!s.wires.length) return '仿真：图纸是空的';
  let txt='仿真：tick '+st.tick+'　元件 '+s.comps.length+'　网络 '+NET.nets.length;
  const cf=countConflicts(); if(cf) txt+='　⚠ 驱动冲突 '+cf;
  return txt; }

/* ------------------------------------------------------------ canvas */
const cv=$('#cv'), ctx=cv.getContext('2d');
let DPR=1;
function resize(){ const r=$('#canvasHost').getBoundingClientRect();
  DPR=Math.min(devicePixelRatio||1,2); cv.width=Math.max(1,Math.round(r.width*DPR)); cv.height=Math.max(1,Math.round(r.height*DPR)); draw(); }
const w2d = p => ({ x:p.x*view.scale+view.ox, y:p.y*view.scale+view.oy });
const d2w = p => ({ x:(p.x-view.ox)/view.scale, y:(p.y-view.oy)/view.scale });
const toCanvas = ev => { const r=cv.getBoundingClientRect(); return { x:ev.clientX-r.left, y:ev.clientY-r.top }; };
function draw(){
  const t=T(), W=cv.width, H=cv.height;
  ctx.setTransform(1,0,0,1,0,0); ctx.clearRect(0,0,W,H); ctx.fillStyle=t.bg; ctx.fillRect(0,0,W,H);
  ctx.save(); ctx.setTransform(DPR,0,0,DPR,0,0); ctx.translate(view.ox,view.oy); ctx.scale(view.scale,view.scale);
  const vis=visibleWorld();
  drawGrid(t,vis);
  sheet().wires.forEach(w=>drawWire(t,w));
  if(st.wireStart&&st.mouse){ const a=st.wireStart.pos; let b=st.mouse; if(!st.noSnap) b={x:Math.round(b.x/10)*10,y:Math.round(b.y/10)*10};
    drawPath([a,...zRoute(a,b)],t.idle,1.5/view.scale,[6,4]); }
  sheet().comps.forEach(c=>drawComp(t,c,false));
  if(st.placing) drawComp(t,st.placing,true);
  drawNotes(t);
  ctx.restore();
  drawOverlay(t);
}
function visibleWorld(){ const tl=d2w({x:0,y:0}), br=d2w({x:cv.width/DPR,y:cv.height/DPR}); return {x:tl.x,y:tl.y,w:br.x-tl.x,h:br.y-tl.y}; }
function drawGrid(t,vis){ if(!opts.showGrid) return; ctx.lineWidth=1/view.scale;
  if(view.scale>=0.5){ ctx.strokeStyle=t.grid; lineSet(10,vis); } ctx.strokeStyle=t.gmajor; lineSet(50,vis); }
function lineSet(step,vis){ if((vis.w/step)+(vis.h/step)>1500) return;
  ctx.beginPath(); const x0=Math.floor(vis.x/step)*step, y0=Math.floor(vis.y/step)*step;
  for(let x=x0;x<=vis.x+vis.w;x+=step){ ctx.moveTo(x,vis.y); ctx.lineTo(x,vis.y+vis.h); }
  for(let y=y0;y<=vis.y+vis.h;y+=step){ ctx.moveTo(vis.x,y); ctx.lineTo(vis.x+vis.w,y); } ctx.stroke(); }
const zRoute=(a,b)=>{ if(Math.round(a.y)===Math.round(b.y)) return [b]; const mx=(a.x+b.x)/2; return [{x:mx,y:a.y},{x:mx,y:b.y},b]; };
function drawPath(pts,color,w,dash){ if(!pts||pts.length<2) return; ctx.save(); ctx.strokeStyle=color; ctx.lineWidth=w;
  if(dash) ctx.setLineDash(dash); ctx.lineCap='round'; ctx.lineJoin='round'; ctx.beginPath();
  ctx.moveTo(pts[0].x,pts[0].y); for(let i=1;i<pts.length;i++) ctx.lineTo(pts[i].x,pts[i].y); ctx.stroke(); ctx.restore(); }
function drawWire(t,w){ const pts=w.path; if(!pts) return; const col=w.sel?t.sel:sigColor(w.signal);
  drawPath(pts,col,w.sel?2.7:2); if(w.sel){ ctx.fillStyle=t.sel; [pts[0],pts[pts.length-1]].forEach(p=>{ ctx.beginPath(); ctx.arc(p.x,p.y,3,0,7); ctx.fill(); }); } }
function drawNotes(t){ sheet().notes.forEach(n=>{ ctx.save(); ctx.strokeStyle=t.dim; ctx.lineWidth=1.4/view.scale;
  ctx.fillStyle=t.text; const fam=getComputedStyle(document.body).fontFamily;
  if(n.kind==='line'){ ctx.beginPath(); ctx.moveTo(n.x,n.y); ctx.lineTo(n.x2,n.y2); ctx.stroke(); }
  else if(n.kind==='rect'){ ctx.strokeRect(n.x,n.y,n.w,n.h); }
  else if(n.kind==='circle'){ ctx.beginPath(); ctx.arc(n.x,n.y,Math.abs(n.r),0,7); ctx.stroke(); }
  else { ctx.font='13px '+fam; ctx.textAlign='left'; ctx.textBaseline='alphabetic'; ctx.fillText(n.text,n.x,n.y); }
  ctx.restore(); }); }

function applyT(c){ const d=defOf(c), cx=d.w/2, cy=d.h/2;
  ctx.translate(c.x+cx,c.y+cy); ctx.rotate(c.rot*Math.PI/180); if(c.mirror) ctx.scale(-1,1); ctx.translate(-cx,-cy); }
function roundRect(x,y,w,h,r){ r=Math.min(r,w/2,h/2); ctx.beginPath(); ctx.moveTo(x+r,y);
  ctx.arcTo(x+w,y,x+w,y+h,r); ctx.arcTo(x+w,y+h,x,y+h,r); ctx.arcTo(x,y+h,x,y,r); ctx.arcTo(x,y,x+w,y,r); ctx.closePath(); }
function drawComp(t,c,ghost){
  ctx.save(); if(ghost) ctx.globalAlpha=.5;
  ctx.save(); applyT(c); ctx.lineWidth=1.6; ctx.lineJoin='round'; ctx.lineCap='round'; drawBody(t,c,defOf(c)); ctx.restore();
  if(!ghost){
    defOf(c).pins.forEach((p,i)=>{ const w=pinWorld(c,i); const hot=st.hoverPin&&st.hoverPin.c===c.id&&st.hoverPin.i===i;
      ctx.fillStyle=hot?t.pinHot:t.pin; ctx.beginPath(); ctx.arc(w.x,w.y,hot?3.4:2.6,0,7); ctx.fill();
      if(opts.showPinNames){ ctx.fillStyle=t.dim; ctx.font=(8/view.scale)+'px '+getComputedStyle(document.body).fontFamily;
        ctx.textAlign='left'; ctx.fillText(p.n,w.x+4,w.y-4); } });
    if(sheet().sel.has(c.id)){ const b=compBounds(c); ctx.save(); ctx.strokeStyle=t.sel; ctx.lineWidth=1.4/view.scale;
      roundRect(b.x-3,b.y-3,b.w+6,b.h+6,5); ctx.stroke(); ctx.restore(); }
    if(opts.showLabels&&c.label){ const b=compBounds(c); ctx.fillStyle=t.text; ctx.textAlign='center';
      ctx.font=(11/view.scale)+'px '+getComputedStyle(document.body).fontFamily; ctx.fillText(c.label,b.x+b.w/2,b.y-5/view.scale); }
  }
  ctx.restore();
}
function drawBody(t,c,d){
  ctx.strokeStyle=t.stroke; ctx.fillStyle=t.fill;
  if(d.kind==='gate'){
    if(d.shape==='buf'){ ctx.beginPath(); ctx.moveTo(0,0); ctx.lineTo(50,25); ctx.lineTo(0,50); ctx.closePath(); ctx.fill(); ctx.stroke();
      if(d.inv) bubble(t,55,25,5); return; }
    if(d.shape==='and'){ const w=80,h=60,r=30,sx=w-r; ctx.beginPath(); ctx.moveTo(0,0); ctx.lineTo(sx,0);
      ctx.arcTo(w,0,w,r,r); ctx.arcTo(w,h,w-r,h,r); ctx.lineTo(0,h); ctx.closePath(); ctx.fill(); ctx.stroke(); if(d.inv) bubble(t,84,30,4); return; }
    if(d.shape==='or'||d.shape==='xor'){ if(d.shape==='xor'){ ctx.beginPath(); ctx.moveTo(-6,0); ctx.bezierCurveTo(8,15,8,45,-6,60); ctx.stroke(); }
      ctx.beginPath(); ctx.moveTo(0,0); ctx.bezierCurveTo(20,7,48,20,80,30); ctx.bezierCurveTo(48,40,20,53,0,60);
      ctx.bezierCurveTo(16,45,16,15,0,0); ctx.closePath(); ctx.fill(); ctx.stroke(); if(d.inv) bubble(t,84,30,4); return; }
    if(d.shape==='mux'){ ctx.beginPath(); ctx.moveTo(0,0); ctx.lineTo(40,0); ctx.lineTo(50,20); ctx.lineTo(50,60); ctx.lineTo(40,80);
      ctx.lineTo(0,80); ctx.closePath(); ctx.fill(); ctx.stroke(); ctx.fillStyle=t.dim; ctx.font='9px '+getComputedStyle(document.body).fontFamily;
      ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.fillText('MUX',22,40); ctx.textBaseline='alphabetic'; return; }
    if(d.shape==='box'){ roundRect(0,0,70,80,3); ctx.fill(); ctx.stroke(); ctx.fillStyle=t.dim; ctx.textAlign='center';
      ctx.textBaseline='middle'; ctx.font='11px '+getComputedStyle(document.body).fontFamily; ctx.fillText('FA',35,40); ctx.textBaseline='alphabetic'; return; }
  }
  if(d.kind==='port'){ if(d.port==='in'){ ctx.beginPath(); ctx.moveTo(0,0); ctx.lineTo(30,0); ctx.lineTo(40,15); ctx.lineTo(30,30); ctx.lineTo(0,30); }
    else { ctx.beginPath(); ctx.moveTo(40,0); ctx.lineTo(10,0); ctx.lineTo(0,15); ctx.lineTo(10,30); ctx.lineTo(40,30); } ctx.closePath(); ctx.fill(); ctx.stroke(); return; }
  if(d.kind==='const'){
    if(c.type==='ground'){ ctx.lineWidth=1.8; ctx.beginPath(); ctx.moveTo(15,0); ctx.lineTo(15,18); ctx.moveTo(2,18); ctx.lineTo(28,18);
      ctx.moveTo(7,23); ctx.lineTo(23,23); ctx.moveTo(12,28); ctx.lineTo(18,28); ctx.stroke(); return; }
    roundRect(0,0,40,24,3); ctx.fill(); ctx.stroke(); ctx.fillStyle=t.text; ctx.font='12px '+getComputedStyle(document.body).fontFamily;
    ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.fillText(d.val,20,12); ctx.textBaseline='alphabetic'; return; }
  if(d.kind==='led'){ const sig=ledSignal(c); ctx.lineWidth=1.6; ctx.beginPath(); ctx.moveTo(15,40); ctx.lineTo(15,30); ctx.stroke();
    ctx.beginPath(); ctx.arc(15,20,10,0,7); ctx.fillStyle= sig==='1'?t.s1:t.ledOff; ctx.fill();
    if(sig==='1'){ ctx.save(); ctx.globalAlpha=.3; ctx.fillStyle=t.s1; ctx.beginPath(); ctx.arc(15,20,15,0,7); ctx.fill(); ctx.restore(); }
    ctx.strokeStyle=t.stroke; ctx.beginPath(); ctx.arc(15,20,10,0,7); ctx.stroke(); return; }
  if(d.kind==='clock'){ roundRect(0,0,60,40,3); ctx.fill(); ctx.stroke(); ctx.beginPath(); ctx.moveTo(6,30); ctx.lineTo(16,30);
    ctx.lineTo(16,10); ctx.lineTo(28,10); ctx.lineTo(28,30); ctx.lineTo(40,30); ctx.lineTo(40,10); ctx.lineTo(52,10); ctx.lineTo(52,30); ctx.stroke(); return; }
  if(d.kind==='dff'){ roundRect(0,0,70,80,3); ctx.fill(); ctx.stroke(); ctx.beginPath(); ctx.moveTo(0,58); ctx.lineTo(10,63); ctx.lineTo(0,68); ctx.stroke();
    ctx.fillStyle=t.dim; ctx.font='10px '+getComputedStyle(document.body).fontFamily; ctx.textAlign='center';
    ctx.fillText('D',13,25); ctx.fillText('Q',57,25); return; }
  if(d.kind==='splitter'){ roundRect(0,0,50,60,3); ctx.fill(); ctx.stroke(); ctx.fillStyle=t.dim; ctx.textAlign='center';
    ctx.textBaseline='middle'; ctx.font='9px '+getComputedStyle(document.body).fontFamily; ctx.fillText('SPL',25,30); ctx.textBaseline='alphabetic'; return; }
  if(d.kind==='tunnel'){ ctx.beginPath(); ctx.moveTo(0,0); ctx.lineTo(52,0); ctx.lineTo(60,15); ctx.lineTo(52,30); ctx.lineTo(0,30); ctx.closePath();
    ctx.fill(); ctx.stroke(); if(c.label){ ctx.fillStyle=t.text; ctx.textAlign='center'; ctx.textBaseline='middle';
      ctx.font='10px '+getComputedStyle(document.body).fontFamily; ctx.fillText(c.label,26,15); ctx.textBaseline='alphabetic'; } return; }
  roundRect(0,0,d.w,d.h,3); ctx.fill(); ctx.stroke();
}
function bubble(t,x,y,r){ ctx.beginPath(); ctx.arc(x,y,r,0,7); ctx.fillStyle=t.bg; ctx.fill(); ctx.strokeStyle=t.stroke; ctx.stroke(); }
function ledSignal(c){ const net=NET.netOf.get(key(c.id,0)); return net===undefined?'Z':NET.value[net]; }
function drawOverlay(t){
  if(st.drag&&st.drag.mode==='band'){ const a=w2d(st.drag.a), b=w2d(st.drag.b);
    ctx.save(); ctx.setLineDash([5,4]); ctx.strokeStyle=t.sel; ctx.lineWidth=1; ctx.fillStyle=t.selFill;
    const x=Math.min(a.x,b.x),y=Math.min(a.y,b.y),w=Math.abs(b.x-a.x),h=Math.abs(b.y-a.y);
    ctx.fillRect(x,y,w,h); ctx.strokeRect(x,y,w,h); ctx.restore(); }
  if(st.drag&&st.drag.mode==='shape'){ const a=w2d(st.drag.a), b=w2d(st.drag.b);
    ctx.save(); ctx.setLineDash([5,4]); ctx.strokeStyle=t.sel; ctx.lineWidth=1.2;
    if(st.draw==='rect') ctx.strokeRect(a.x,a.y,b.x-a.x,b.y-a.y);
    else if(st.draw==='circle'){ const r=Math.hypot(b.x-a.x,b.y-a.y); ctx.beginPath(); ctx.arc(a.x,a.y,r,0,7); ctx.stroke(); }
    else { ctx.beginPath(); ctx.moveTo(a.x,a.y); ctx.lineTo(b.x,b.y); ctx.stroke(); } ctx.restore(); }
}

/* ------------------------------------------------------------ hit testing */
function distSeg(p,a,b){ const dx=b.x-a.x, dy=b.y-a.y, l=dx*dx+dy*dy||1; let t=((p.x-a.x)*dx+(p.y-a.y)*dy)/l; t=clamp(t,0,1);
  return Math.hypot(p.x-(a.x+t*dx),p.y-(a.y+t*dy)); }
function pick(dev){ const w=d2w(dev), s=sheet();
  for(let ci=s.comps.length-1;ci>=0;ci--){ const c=s.comps[ci], d=defOf(c);
    for(let i=0;i<d.pins.length;i++){ const p=pinWorld(c,i); if(Math.hypot(p.x-w.x,p.y-w.y) <= 6/view.scale+2) return {kind:'pin',comp:c,pin:i,pos:p}; } }
  for(let ci=s.comps.length-1;ci>=0;ci--){ const c=s.comps[ci], b=compBounds(c);
    if(w.x>=b.x-2&&w.x<=b.x+b.w+2&&w.y>=b.y-2&&w.y<=b.y+b.h+2) return {kind:'comp',comp:c}; }
  for(let wi=s.wires.length-1;wi>=0;wi--){ const wire=s.wires[wi], pts=wire.path; if(!pts) continue;
    for(let i=0;i<pts.length-1;i++) if(distSeg(w,pts[i],pts[i+1]) <= 5/view.scale+2) return {kind:'wire',wire}; }
  for(let ni=s.notes.length-1;ni>=0;ni--){ const n=s.notes[ni]; if(n.kind==='text'&&Math.abs(n.x-w.x)<80&&Math.abs(n.y-w.y)<16) return {kind:'note',note:n}; }
  return null; }

/* ------------------------------------------------------------ selection */
function clearSel(){ const s=sheet(); s.sel.clear(); s.wires.forEach(w=>w.sel=false); syncSelection(); }
function selectComp(c,add){ const s=sheet(); if(!add){ s.sel.clear(); s.wires.forEach(w=>w.sel=false); } s.sel.add(c.id); syncSelection(); }
function selectWire(w,add){ const s=sheet(); if(!add){ s.sel.clear(); s.wires.forEach(x=>x.sel=false); } w.sel=true; syncSelection(); }
function syncSelection(){
  const s=sheet(), comps=s.comps.filter(c=>s.sel.has(c.id)), wires=s.wires.filter(w=>w.sel);
  if(comps.length===1&&wires.length===0){ st.selectedKind='comp'; st.selectedId=comps[0].id; }
  else if(wires.length===1&&comps.length===0){ st.selectedKind='wire'; st.selectedId=wires[0].id; }
  else if(!comps.length&&!wires.length){ st.selectedKind=null; st.selectedId=null; }
  else { st.selectedKind='multi'; st.selectedId=null; }
  renderProp(); renderStatus(); renderTree(); draw();
}

/* ------------------------------------------------------------ pointer input */
let spaceDown=false;
cv.addEventListener('pointerdown', onDown);
addEventListener('pointermove', onMove);
addEventListener('pointerup', onUp);
cv.addEventListener('wheel', onWheel, {passive:false});
cv.addEventListener('contextmenu', e=>e.preventDefault());
function snapWorld(dev){ const w=d2w(dev); const on=!st.noSnap&&st.tool!=='Drag';
  return on?{x:Math.round(w.x/10)*10,y:Math.round(w.y/10)*10}:{x:Math.round(w.x),y:Math.round(w.y)}; }
function onDown(e){
  const dev=toCanvas(e); st.mouse=d2w(dev); st.noSnap=e.shiftKey;
  if(cv.setPointerCapture) try{cv.setPointerCapture(e.pointerId);}catch(_){}
  if(e.button===1||(e.button===0&&(st.tool==='Drag'||spaceDown))){ st.drag={mode:'pan',a:dev,ox:view.ox,oy:view.oy}; return; }
  if(e.button!==0) return;
  const hit=pick(dev);
  if(st.placing||st.tool==='Component'){ placeAt(dev); return; }
  if(st.tool==='Wire'){ wireClick(hit); return; }
  if(st.tool==='Eraser'){ if(hit) eraseHit(hit); return; }
  if(st.tool==='Text'){ if(hit&&hit.kind==='comp') editLabel(hit.comp); else addNote(snapWorld(dev)); return; }
  if(st.tool==='Drawing'){ st.drag={mode:'shape',a:d2w(dev),b:d2w(dev)}; return; }
  if(hit&&hit.kind==='comp'){ const wasSel=sheet().sel.has(hit.comp.id);
    if(e.shiftKey) selectComp(hit.comp,true); else if(!wasSel) selectComp(hit.comp,false);
    st.drag={mode:'move',a:d2w(dev),orig:sheet().comps.filter(c=>sheet().sel.has(c.id)).map(c=>({c,ox:c.x,oy:c.y})),moved:false}; return; }
  if(hit&&hit.kind==='wire'){ selectWire(hit.wire,e.shiftKey); return; }
  if(hit&&hit.kind==='note'){ editNote(hit.note); return; }
  if(!e.shiftKey) clearSel();
  st.drag={mode:'band',a:dev,b:dev};
}
function onMove(e){
  const dev=toCanvas(e); const w=d2w(dev); st.mouse=w; st.noSnap=e.shiftKey;
  if(st.drag){
    if(st.drag.mode==='pan'){ view.ox=st.drag.ox+(dev.x-st.drag.a.x); view.oy=st.drag.oy+(dev.y-st.drag.a.y); draw(); return; }
    if(st.drag.mode==='band'){ st.drag.b=dev; draw(); return; }
    if(st.drag.mode==='shape'){ st.drag.b=w; draw(); return; }
    if(st.drag.mode==='move'){ let dx=w.x-st.drag.a.x, dy=w.y-st.drag.a.y; st.drag.moved=st.drag.moved||(Math.abs(dx)+Math.abs(dy)>2);
      if(!st.noSnap){ dx=Math.round(dx/10)*10; dy=Math.round(dy/10)*10; }
      st.drag.orig.forEach(o=>{ o.c.x=o.ox+dx; o.c.y=o.oy+dy; }); refreshWirePaths(); draw(); return; }
  }
  const hit=pick(dev); const prev=st.hoverPin;
  st.hoverPin = hit&&hit.kind==='pin' ? {c:hit.comp.id,i:hit.pin} : null;
  cv.style.cursor = st.placing||st.tool==='Component' ? 'copy' : st.tool==='Wire' ? (hit&&hit.kind==='pin'?'crosshair':'default')
    : st.tool==='Eraser' ? 'not-allowed' : st.tool==='Text' ? 'text' : hit ? 'pointer' : (st.tool==='Drag'?'grab':'default');
  const changed = !!st.hoverPin || (prev&&st.hoverPin!==prev) || st.placing;
  if(changed||st.wireStart) draw();
  else if(prev||hit) draw();
  if(st.placing){ const p=snapWorld(dev); st.placing.x=p.x; st.placing.y=p.y; draw(); }
}
function onUp(e){
  if(st.drag){
    if(st.drag.mode==='band'){ const a=st.drag.a,b=st.drag.b;
      if(Math.abs(b.x-a.x)>4||Math.abs(b.y-a.y)>4){ sheet().comps.forEach(c=>{ const bb=compBounds(c); const ctr=w2d({x:bb.x+bb.w/2,y:bb.y+bb.h/2});
        if(ctr.x>=Math.min(a.x,b.x)&&ctr.x<=Math.max(a.x,b.x)&&ctr.y>=Math.min(a.y,b.y)&&ctr.y<=Math.max(a.y,b.y)) sheet().sel.add(c.id); }); syncSelection(); } }
    if(st.drag.mode==='shape'){ const a=st.drag.a,b=st.drag.b;
      if(Math.hypot(b.x-a.x,b.y-a.y)>4){ if(st.draw==='line') sheet().notes.push({kind:'line',x:a.x,y:a.y,x2:b.x,y2:b.y});
        else if(st.draw==='rect') sheet().notes.push({kind:'rect',x:Math.min(a.x,b.x),y:Math.min(a.y,b.y),w:Math.abs(b.x-a.x),h:Math.abs(b.y-a.y)});
        else sheet().notes.push({kind:'circle',x:a.x,y:a.y,r:Math.hypot(b.x-a.x,b.y-a.y)}); markDirty(); } }
    if(st.drag.mode==='move'&&st.drag.moved) markDirty();
  }
  st.drag=null; draw();
}
function onWheel(e){ e.preventDefault(); zoomAt(view.scale*(e.deltaY<0?1.12:1/1.12), toCanvas(e)); }
function zoomAt(ns, dev){ ns=clamp(ns,0.25,4); const w=d2w(dev); view.scale=ns; view.ox=dev.x-w.x*ns; view.oy=dev.y-w.y*ns; renderZoom(); draw(); }
function zoomFit(){ const s=sheet();
  if(!s.comps.length&&!s.wires.length){ view.scale=1; view.ox=40; view.oy=30; renderZoom(); draw(); return; }
  let mnx=1e9,mny=1e9,mxx=-1e9,mxy=-1e9;
  s.comps.forEach(c=>{ const b=compBounds(c); mnx=Math.min(mnx,b.x); mny=Math.min(mny,b.y); mxx=Math.max(mxx,b.x+b.w); mxy=Math.max(mxy,b.y+b.h); });
  s.wires.forEach(w=>(w.path||[]).forEach(p=>{ mnx=Math.min(mnx,p.x); mny=Math.min(mny,p.y); mxx=Math.max(mxx,p.x); mxy=Math.max(mxy,p.y); }));
  const host=$('#canvasHost').getBoundingClientRect(), pad=80;
  const sc=clamp(Math.min((host.width-pad)/Math.max(1,mxx-mnx),(host.height-pad)/Math.max(1,mxy-mny)),0.25,3);
  view.scale=sc; view.ox=(host.width-(mnx+mxx)*sc)/2; view.oy=(host.height-(mny+mxy)*sc)/2; renderZoom(); draw(); }

/* ------------------------------------------------------------ placement / wiring */
function beginPlacement(type){ const d=DEF[type];
  st.placing={ id:-1, type, x:200, y:200, rot:0, mirror:false, label:'', val:d.val||'', inState: type==='input'?'1':null };
  st.tool='Component'; renderToolbar(); renderToolbox(); log('放置元件：'+d.name+'（点击画布放置，Esc 取消）'); draw(); }
function placeAt(dev){ if(!st.placing){ log('请先在左侧元件库中选择一个元件。','warn'); return; } const p=snapWorld(dev); const d=DEF[st.placing.type];
  const c=mkComp(st.placing.type,p.x,p.y,'',undefined);
  if(st.placing.type==='input'){ c.label=String.fromCharCode(65+(sheet().comps.filter(x=>x.type==='input').length%26)); c.inState='1'; }
  sheet().comps.push(c); st.placing=null; st.tool='Select'; markDirty(); evaluate(); syncSelection(); renderToolbar(); renderToolbox();
  log('已放置：'+d.name+(c.label?(' 「'+c.label+'」'):'')); }
function pinName(c,i){ return (c.label||defOf(c).name)+'.'+defOf(c).pins[i].n; }
function wireClick(hit){ if(!st.wireStart){ if(hit&&hit.kind==='pin'){ st.wireStart={comp:hit.comp.id,pin:hit.pin,pos:hit.pos}; log('导线起点：'+pinName(hit.comp,hit.pin)); draw(); }
    else log('导线工具：请先点击一个引脚。','warn'); return; }
  if(hit&&hit.kind==='pin'){ const a=st.wireStart;
    if(hit.comp.id===a.comp&&hit.pin===a.pin){ st.wireStart=null; draw(); return; }
    sheet().wires.push(mkWire(a.comp,a.pin,hit.comp.id,hit.pin)); st.wireStart=null; markDirty(); evaluate(); syncSelection();
    log('已连线：'+pinName(compById(a.comp),a.pin)+' → '+pinName(hit.comp,hit.pin)); return; }
  st.wireStart=null; draw();
}
function eraseHit(hit){ const s=sheet();
  if(hit.kind==='comp'){ s.comps=s.comps.filter(c=>c!==hit.comp); s.wires=s.wires.filter(w=>w.a.c!==hit.comp.id&&w.b.c!==hit.comp.id); log('已删除元件：'+(hit.comp.label||defOf(hit.comp).name)); }
  else if(hit.kind==='wire'){ s.wires=s.wires.filter(w=>w!==hit.wire); log('已删除导线'); }
  else if(hit.kind==='note'){ s.notes=s.notes.filter(n=>n!==hit.note); }
  markDirty(); evaluate(); syncSelection(); }
function addNote(p){ const n={kind:'text',x:p.x,y:p.y,text:'注释'}; sheet().notes.push(n); markDirty(); editNote(n); }
function editNote(n){ inlineEdit({ wx:n.x, wy:n.y, value:n.text, onDone:v=>{ n.text=v||'注释'; markDirty(); draw(); } }); }
function editLabel(c){ const b=compBounds(c); inlineEdit({ wx:b.x, wy:b.y-6, value:c.label, onDone:v=>{ c.label=v; markDirty(); renderTree(); renderToolbox(); draw(); } }); }
function inlineEdit({wx,wy,value,onDone}){ const host=$('#canvasHost'); const d=w2d({x:wx,y:wy});
  const inp=document.createElement('input'); inp.className='inline-edit'; inp.value=value||'';
  inp.style.left=Math.max(2,d.x)+'px'; inp.style.top=Math.max(2,d.y-12)+'px'; host.appendChild(inp); inp.focus(); inp.select();
  const done=commit=>{ if(inp._done) return; inp._done=true; const v=inp.value; inp.remove(); if(commit) onDone(v); };
  inp.addEventListener('keydown',e=>{ if(e.key==='Enter') done(true); else if(e.key==='Escape') done(false); e.stopPropagation(); });
  inp.addEventListener('blur',()=>done(true));
}

/* ------------------------------------------------------------ toolbar */
const ICON = {
  cursor:'<path d="M5 3l6 16 2.2-6.2L19 11z"/>',
  hand:'<path d="M8 13V6a1.6 1.6 0 0 1 3.2 0v5M11.2 11V4.6a1.6 1.6 0 0 1 3.2 0V11M14.4 11V6a1.6 1.6 0 0 1 3.2 0v7.5"/><path d="M8 12.5 6.6 11A1.7 1.7 0 0 0 4 12a1.7 1.7 0 0 0 .3 1L8 19.5c.7 1.5 2 2.5 4 2.5h.6c3.4 0 5.4-2.3 5.4-5.5"/>',
  text:'<path d="M5 6h14M12 6v13M9 19h6"/>',
  chip:'<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3"/>',
  wire:'<circle cx="6" cy="18" r="2.4"/><circle cx="18" cy="6" r="2.4"/><path d="M8 17c2-1 3-3 4-5s3-4 4-4"/>',
  shapes:'<rect x="4" y="4" width="9" height="9" rx="1.5"/><circle cx="15" cy="15" r="5"/>',
  eraser:'<path d="M4 16l7-7 6 6-4 4H7z"/><path d="M11 9l4-4a2 2 0 0 1 3 0l3 3a2 2 0 0 1 0 3l-4 4"/><path d="M6 20h13"/>',
  rotate:'<path d="M20 12a8 8 0 1 1-2.3-5.6"/><path d="M20 4v4h-4"/>',
  mirror:'<path d="M12 3v18"/><path d="M8 7 3 12l5 5zM16 7l5 5-5 5z"/>',
  zoomin:'<circle cx="11" cy="11" r="7"/><path d="M11 8v6M8 11h6M20 20l-3.2-3.2"/>',
  zoomout:'<circle cx="11" cy="11" r="7"/><path d="M8 11h6M20 20l-3.2-3.2"/>',
  fit:'<path d="M4 9V5a1 1 0 0 1 1-1h4M20 9V5a1 1 0 0 0-1-1h-4M4 15v4a1 1 0 0 0 1 1h4M20 15v4a1 1 0 0 1-1 1h-4"/>',
  play:'<path d="M7 5l12 7-12 7z"/>',
  step:'<path d="M5 5v14M9 5l11 7-11 7z"/>',
  reset:'<path d="M4 12a8 8 0 1 0 2.4-5.7"/><path d="M4 4v4h4"/>'
};
const icon = n => '<svg class="ico" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">'+ICON[n]+'</svg>';
const TOOLS = [
  ['Select','cursor','选择','S','点选 / 框选 / 拖动'],
  ['Drag','hand','拖拽','H','平移画布'],
  ['Text','text','文本','T','添加注释 / 改标签'],
  ['Component','chip','元件','C','放置元件'],
  ['Wire','wire','导线','W','连接引脚'],
  ['Drawing','shapes','绘图','D','画装饰图形'],
  ['Eraser','eraser','擦除','E','点擦 / 框擦']
];
const sepEl = () => { const d=document.createElement('span'); d.className='tb-sep'; return d; };
function tbtn(ic,fn,title){ const b=document.createElement('button'); b.type='button'; b.className='tbtn'; b.title=title; b.innerHTML=icon(ic); b.onclick=fn; return b; }
function renderToolbar(){
  const el=$('#toolbar'); el.innerHTML='';
  const seg=document.createElement('div'); seg.className='seg'; seg.setAttribute('role','group'); seg.setAttribute('aria-label','工具');
  TOOLS.forEach(([id,ic,label,key])=>{ const b=document.createElement('button'); b.type='button'; b.title=label+'（'+key+'）';
    b.setAttribute('aria-pressed',String(st.tool===id)); b.innerHTML=icon(ic)+'<span class="lbl">'+label+'</span>'; b.onclick=()=>setTool(id); seg.appendChild(b); });
  el.appendChild(seg); el.appendChild(sepEl());
  el.appendChild(tbtn('rotate',()=>act('rotate'),'顺时针旋转 90° (Ctrl+R)'));
  el.appendChild(tbtn('mirror',()=>act('mirror'),'水平镜像 (Ctrl+M)'));
  el.appendChild(sepEl());
  el.appendChild(tbtn('zoomout',()=>act('zoomout'),'缩小'));
  const zv=document.createElement('span'); zv.className='zoom-val'; zv.id='zoomVal'; zv.textContent=Math.round(view.scale*100)+'%'; el.appendChild(zv);
  el.appendChild(tbtn('zoomin',()=>act('zoomin'),'放大'));
  el.appendChild(tbtn('fit',()=>act('fit'),'适应窗口 (Ctrl+0)'));
  el.appendChild(sepEl());
  const run=tbtn('play',()=>act('run'),'运行自动节拍 (F5)'); run.classList.add('primary'); run.id='runBtn'; run.setAttribute('aria-pressed',String(st.simRunning)); el.appendChild(run);
  el.appendChild(tbtn('step',()=>act('step'),'单步 (Ctrl+T)'));
  el.appendChild(tbtn('reset',()=>act('reset'),'复位 (Ctrl+Shift+T)'));
  el.appendChild(Object.assign(document.createElement('div'),{className:'spacer'}));
  const lab=document.createElement('span'); lab.style.cssText='font-size:11.5px;color:var(--text-3)'; lab.textContent='节拍'; el.appendChild(lab);
  const sel=document.createElement('select'); sel.className='freq-select'; sel.id='freqSel'; sel.setAttribute('aria-label','节拍频率');
  [1,2,5,10,20].forEach(hz=>{ const o=document.createElement('option'); o.value=hz; o.textContent=hz+' Hz'; if(hz===st.simHz) o.selected=true; sel.appendChild(o); });
  sel.onchange=()=>{ st.simHz=+sel.value; log('节拍频率设为 '+st.simHz+' Hz。'); }; el.appendChild(sel);
  renderZoom();
}
function setTool(id){ st.tool=id; if(id!=='Component') st.placing=null; st.wireStart=null; renderToolbar(); renderStatus(); draw();
  const t=TOOLS.find(x=>x[0]===id); log('工具：'+t[2]); }
function renderZoom(){ const z=$('#zoomVal'); if(z) z.textContent=Math.round(view.scale*100)+'%';
  const f=$('#fabZoomReset'); if(f) f.textContent=Math.round(view.scale*100)+'%';
  const b=$('#stZoom'); if(b) b.textContent='缩放 '+Math.round(view.scale*100)+'%'; }

/* ------------------------------------------------------------ menu bar */
const MENUS = {
  '文件':[ ['新建工程','Ctrl+N',()=>act('new')],['打开工程…','Ctrl+O',()=>act('open')],['保存','Ctrl+S',()=>act('save')],
    ['另存为…','Ctrl+Shift+S',()=>act('saveas')],'sep',['打开 VCD 波形…','',()=>act('vcd')],'sep',['退出','Alt+F4',()=>act('exit')] ],
  '编辑':[ ['撤销','Ctrl+Z',()=>act('undo')],['重做','Ctrl+Y',()=>act('redo')],'sep',['剪切','Ctrl+X',()=>notImplemented()],
    ['复制','Ctrl+C',()=>notImplemented()],['粘贴','Ctrl+V',()=>notImplemented()],['删除','Del',()=>act('delete')],'sep',['全选','Ctrl+A',()=>act('selectall')],'sep',['偏好设置…','',()=>notImplemented()] ],
  '视图':[ ['放大','Ctrl+=',()=>act('zoomin')],['缩小','Ctrl+-',()=>act('zoomout')],['适应窗口','Ctrl+0',()=>act('fit')],['实际大小 100%','Ctrl+1',()=>act('zoomreset')],'sep',
    ['显示网格','',()=>act('grid'),'check',()=>opts.showGrid],['显示引脚名','',()=>act('pinnames'),'check',()=>opts.showPinNames],['显示元件标签','',()=>act('labels'),'check',()=>opts.showLabels],'sep',
    ['元件库','',()=>togglePanel('left'),'check',()=>!$('#panelLeft').hidden],['属性 / 层次树','',()=>togglePanel('right'),'check',()=>!$('#panelRight').hidden],['波形 / 消息','',()=>togglePanel('bottom'),'check',()=>!$('#panelBottom').hidden] ],
  '工具':TOOLS.map(t=>[t[2],t[3],()=>setTool(t[0]),'check',()=>st.tool===t[0]]).concat(['sep',
    ['绘图形状：直线','',()=>{st.draw='line';setTool('Drawing');}],['绘图形状：矩形','',()=>{st.draw='rect';setTool('Drawing');}],['绘图形状：圆形','',()=>{st.draw='circle';setTool('Drawing');}],'sep',
    ['顺时针旋转 90°','Ctrl+R',()=>act('rotate')],['逆时针旋转 90°','Ctrl+Shift+R',()=>act('rotateccw')],['水平镜像','Ctrl+M',()=>act('mirror')] ]),
  '仿真':[ ['运行','F5',()=>act('run')],['停止','F6',()=>act('stop')],'sep',['单步','Ctrl+T',()=>act('step')],['复位','Ctrl+Shift+T',()=>act('reset')],'sep',
    ['生成真值表…','Ctrl+U',()=>log('真值表面板属于 S3 里程碑，尚未实现。','warn')] ],
  '图纸':[ ['新建图纸','',()=>act('newsheet')],['关闭当前图纸','',()=>act('closesheet')] ],
  'Verilog':[ ['导出 Verilog','F7',()=>log('导出 Verilog 属于 M3 里程碑：图 → 码生成器尚未接入。','warn')],['从 Verilog 导入','F8',()=>log('从 Verilog 导入属于 M4 里程碑：词法/语法分析与自动布局尚未接入。','warn')],'sep',
    ['双向同步状态…','',()=>log('双向同步：对侧文件（未关联），同步引擎将在 M3 / M4 接入。')],['打开对侧文件','',()=>log('尚未关联 Verilog 对侧文件。','warn')] ],
  '帮助':[ ['快捷键一览','F1',()=>shortcuts()],'sep',['关于 SiliFlow','',()=>about()] ]
};
let openPop=null;
function renderMenubar(){ const bar=$('#menubar'); bar.innerHTML='';
  Object.keys(MENUS).forEach(name=>{ const b=document.createElement('button'); b.className='menu-btn'; b.textContent=name;
    b.setAttribute('aria-expanded','false'); b.setAttribute('aria-haspopup','true');
    b.onclick=e=>{ e.stopPropagation(); openMenu(name,b); }; b.onmouseenter=()=>{ if(openPop) openMenu(name,b); }; bar.appendChild(b); }); }
function openMenu(name,anchor){ closeMenu(); const pop=document.createElement('div'); pop.className='menu-pop open';
  const r=anchor.getBoundingClientRect(), br=$('#menubar').getBoundingClientRect();
  pop.style.left=(r.left-br.left)+'px'; pop.style.top='30px';
  MENUS[name].forEach(it=>{ if(it==='sep'){ const s=document.createElement('div'); s.className='sep'; pop.appendChild(s); return; }
    const [label,keyk,fn,kind,stateFn]=it; const mi=document.createElement('button'); mi.className='mi';
    if(kind==='check'&&stateFn&&stateFn()) mi.classList.add('checked');
    mi.innerHTML='<span>'+label+(kind==='check'?' <span class="ck">✓</span>':'')+'</span>'+(keyk?'<span class="k">'+keyk+'</span>':'');
    mi.onclick=()=>{ closeMenu(); if(fn) fn(); }; pop.appendChild(mi); });
  $('#menubar').appendChild(pop); anchor.setAttribute('aria-expanded','true'); openPop={pop,anchor};
}
function closeMenu(){ if(openPop){ openPop.pop.remove(); openPop.anchor.setAttribute('aria-expanded','false'); openPop=null; } }
document.addEventListener('click',()=>closeMenu());

/* ------------------------------------------------------------ toolbox */
function renderToolbox(){
  const body=$('#toolboxBody'), q=($('#searchInput').value||'').trim().toLowerCase(); body.innerHTML='';
  $('#toolboxCount').textContent='· '+LIB_TOTAL+' 个元件';
  const all=Object.values(DEF);
  if(q){ const hits=all.filter(d=>d.type.includes(q)||d.name.toLowerCase().includes(q)||d.cat.includes(q));
    if(!hits.length){ body.innerHTML='<div class="empty-note">（没有匹配的元件）</div>'; return; }
    const wrap=document.createElement('div'); wrap.className='cat-items'; hits.forEach(d=>wrap.appendChild(compItem(d))); body.appendChild(wrap); return; }
  LIB_CATS.forEach(cat=>{ const box=document.createElement('div'); box.className='cat';
    const head=document.createElement('button'); head.className='cat-head'; head.type='button';
    head.innerHTML='<svg class="ico chev" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg><span>'+cat+'</span>';
    const list=document.createElement('div'); list.className='cat-items';
    all.filter(d=>d.cat===cat).forEach(d=>list.appendChild(compItem(d)));
    head.onclick=()=>box.classList.toggle('collapsed'); box.appendChild(head); box.appendChild(list); body.appendChild(box); });
}
function compItem(d){ const b=document.createElement('button'); b.type='button'; b.className='comp-item';
  b.setAttribute('aria-pressed',String(!!(st.placing&&st.placing.type===d.type)));
  b.innerHTML='<span class="mini">'+miniIcon(d)+'</span><span class="txt"><span class="n">'+d.name+'</span><span class="t">'+d.type+'</span></span>';
  b.onclick=()=>{ beginPlacement(d.type); renderToolbox(); }; return b; }
function miniIcon(d){
  if(d.shape==='and') return '<svg width="20" height="14" viewBox="0 0 20 14"><path d="M2 1h8a6 6 0 0 1 0 12H2z" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
  if(d.shape==='or'||d.shape==='xor') return '<svg width="20" height="14" viewBox="0 0 20 14"><path d="M1 1c3 2 8 3 17 6-9 3-14 4-17 6 3-4 3-8 0-12z" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
  if(d.shape==='buf') return '<svg width="18" height="14" viewBox="0 0 18 14"><path d="M2 1l14 6-14 6z" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
  if(d.kind==='port') return '<svg width="18" height="12" viewBox="0 0 18 12"><path d="M1 1h10l5 5-5 5H1z" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
  if(d.kind==='led') return '<svg width="14" height="16" viewBox="0 0 14 16"><path d="M7 1v6" stroke="currentColor" stroke-width="1.3"/><circle cx="7" cy="10" r="4.5" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
  if(d.kind==='const') return '<svg width="16" height="12" viewBox="0 0 16 12"><rect x="1" y="1" width="14" height="10" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
  return '<svg width="16" height="14" viewBox="0 0 16 14"><rect x="1" y="1" width="14" height="12" rx="2" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
}

/* ------------------------------------------------------------ properties */
function pgroup(title,rows){ const g=document.createElement('div'); g.className='prop-group';
  const t=document.createElement('div'); t.className='pg-t'; t.textContent=title; g.appendChild(t); rows.forEach(r=>r&&g.appendChild(r)); return g; }
function fieldRO(label,val){ const f=document.createElement('div'); f.className='field';
  f.innerHTML='<label>'+label+'</label><div class="val" title="'+String(val).replace(/"/g,'&quot;')+'">'+val+'</div>'; return f; }
function fieldText(label,val,fn){ const f=document.createElement('div'); f.className='field'; f.innerHTML='<label>'+label+'</label>';
  const inp=document.createElement('input'); inp.type='text'; inp.value=val||''; inp.setAttribute('aria-label',label); inp.oninput=()=>fn(inp.value); f.appendChild(inp); return f; }
function fieldNum(label,val,fn){ const f=document.createElement('div'); f.className='field'; f.innerHTML='<label>'+label+'</label>';
  const inp=document.createElement('input'); inp.type='number'; inp.value=val; inp.setAttribute('aria-label',label); inp.oninput=()=>fn(+inp.value||0); f.appendChild(inp); return f; }
function fieldSelect(label,opts2,val,fn){ const f=document.createElement('div'); f.className='field'; f.innerHTML='<label>'+label+'</label>';
  const s=document.createElement('select'); s.setAttribute('aria-label',label);
  opts2.forEach(([v,t])=>{ const o=document.createElement('option'); o.value=v; o.textContent=t; if(v===val) o.selected=true; s.appendChild(o); });
  s.onchange=()=>fn(s.value); f.appendChild(s); return f; }
function fieldSwitch(label,on,fn){ const f=document.createElement('div'); f.className='field'; f.innerHTML='<label>'+label+'</label>';
  const b=document.createElement('button'); b.type='button'; b.className='switch'; b.setAttribute('role','switch');
  b.setAttribute('aria-checked',String(!!on)); b.setAttribute('aria-label',label);
  b.onclick=()=>{ const nv=!(b.getAttribute('aria-checked')==='true'); b.setAttribute('aria-checked',String(nv)); fn(nv); }; f.appendChild(b); return f; }
function fieldSig(label,val){ const f=document.createElement('div'); f.className='field';
  f.innerHTML='<label>'+label+'</label><div class="val"><span class="dot-sig" style="display:inline-block;background:'+sigColor(val)+'"></span> '+val+'</div>'; return f; }
function btn(label,fn,cls){ const b=document.createElement('button'); b.className='btn'+(cls?' '+cls:''); b.textContent=label; b.onclick=fn; return b; }
const dirName = d => d==='in'?'输入':d==='out'?'输出':'双向';
const endText = e => { const c=compById(e.c); return c?('元件 #'+c.id+' 引脚 '+defOf(c).pins[e.p].n):'悬空'; };
function segLen(pts){ if(!pts) return 0; let l=0; for(let i=0;i<pts.length-1;i++) l+=Math.hypot(pts[i+1].x-pts[i].x,pts[i+1].y-pts[i].y); return Math.round(l); }
function renderProp(){
  const head=$('#propHead'), body=$('#propBody'), acts=$('#propActions'); acts.innerHTML=''; body.innerHTML='';
  if(st.selectedKind==='comp'){
    const c=compById(st.selectedId); if(!c){ st.selectedKind=null; return renderProp(); } const d=defOf(c);
    $('#propSub').textContent='元件'; head.innerHTML='<div class="ph-t">'+d.name+'</div><div class="ph-s">'+d.type+'　#'+c.id+'</div>';
    body.appendChild(pgroup('基本',[ fieldRO('类型',c.type),
      fieldText('标签',c.label,v=>{ c.label=v; markDirty(); renderTree(); renderToolbox(); draw(); }),
      fieldText('值',c.val,v=>{ c.val=v; markDirty(); draw(); }) ]));
    body.appendChild(pgroup('位置与朝向',[
      fieldNum('X',c.x,v=>{ c.x=v; refreshWirePaths(); markDirty(); draw(); }),
      fieldNum('Y',c.y,v=>{ c.y=v; refreshWirePaths(); markDirty(); draw(); }),
      fieldSelect('旋转',[['0','0°'],['90','90°'],['180','180°'],['270','270°']],String(c.rot),v=>{ c.rot=+v; refreshWirePaths(); markDirty(); draw(); }),
      fieldSwitch('水平镜像',c.mirror,v=>{ c.mirror=v; refreshWirePaths(); markDirty(); draw(); }) ]));
    if(d.pins.length===1&&(d.kind==='port'||d.kind==='const'||d.kind==='led'||d.kind==='clock')){
      const net=NET.netOf.get(key(c.id,0)), val=net===undefined?'Z':NET.value[net]; const rows=[ fieldSig('当前电平',val) ];
      if(c.type==='input') rows.push(fieldSwitch('强制高电平',c.inState==='1',v=>{ c.inState=v?'1':'0'; evaluate(); draw(); renderProp(); renderStatus(); log('翻转 '+(c.label||'输入')+' → '+(v?'1':'0')); }));
      body.appendChild(pgroup('仿真',rows)); }
    if(d.kind==='gate') body.appendChild(pgroup('Verilog',[ fieldRO('原语',d.gate||d.type), fieldRO('类别',c.type) ]));
    body.appendChild(pgroup('引脚', d.pins.map((p,i)=>{ const w=pinWorld(c,i); return fieldRO(p.n,dirName(p.d)+'  ('+Math.round(w.x)+', '+Math.round(w.y)+')'); })));
    acts.appendChild(btn('旋转 90°',()=>act('rotate'))); acts.appendChild(btn('水平镜像',()=>act('mirror')));
    acts.appendChild(Object.assign(document.createElement('span'),{style:'flex:1'})); acts.appendChild(btn('删除',()=>act('delete'),'danger'));
  } else if(st.selectedKind==='wire'){
    const w=sheet().wires.find(x=>x.id===st.selectedId); if(!w){ st.selectedKind=null; return renderProp(); }
    $('#propSub').textContent='导线'; head.innerHTML='<div class="ph-t">导线</div><div class="ph-s">#'+w.id+'</div>';
    body.appendChild(pgroup('基本',[ fieldRO('总长（px）',String(segLen(w.path))), fieldRO('段数',String(w.path?Math.max(0,w.path.length-1):0)) ]));
    body.appendChild(pgroup('两端',[ fieldRO('起点',endText(w.a)), fieldRO('终点',endText(w.b)) ]));
    acts.appendChild(Object.assign(document.createElement('span'),{style:'flex:1'})); acts.appendChild(btn('删除',()=>act('delete'),'danger'));
  } else if(st.selectedKind==='multi'){
    const s=sheet(), cc=s.comps.filter(c=>s.sel.has(c.id)).length, wc=s.wires.filter(w=>w.sel).length;
    $('#propSub').textContent='多选'; head.innerHTML='<div class="ph-t">多选</div><div class="ph-s">'+cc+' 个元件 / '+wc+' 根导线</div>';
    body.appendChild(pgroup('多选',[ fieldRO('说明','使用下方按钮批量操作，或单选查看详细属性') ]));
    acts.appendChild(Object.assign(document.createElement('span'),{style:'flex:1'})); acts.appendChild(btn('删除',()=>act('delete'),'danger'));
  } else {
    $('#propSub').textContent=''; head.innerHTML='<div class="ph-t">未选中对象</div><div class="ph-s">在画布上点选一个元件或导线</div>';
    body.appendChild(pgroup('提示',[ fieldRO('说明','在画布上点选一个元件或导线') ]));
    acts.innerHTML='<span style="font-size:11.5px;color:var(--text-3);padding:4px 2px">选择对象后可编辑属性</span>';
  }
}

/* ------------------------------------------------------------ hierarchy tree */
function escapeHtml(s){ return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function renderTree(){
  const body=$('#treeBody'); body.innerHTML='<div class="tree" id="treeInner"></div>'; const root=$('#treeInner'), s=sheet();
  addRow(root,1,'工程：'+proj.name,'folder');
  addRow(root,2,'图纸：'+s.name,'doc', s.comps.length+' 元件 / '+s.wires.length+' 导线');
  addRow(root,3,'元件 ('+s.comps.length+')','box');
  s.comps.forEach(c=>{ const d=defOf(c); addRow(root,3,(c.label||'(无名)')+' : '+d.name,'part',null,c.id); });
  addRow(root,3,'网络 ('+NET.nets.length+')','net');
  NET.nets.forEach(n=>addRow(root,3,n.name+'  ('+n.pins.length+' 引脚 / '+n.wires.length+' 导线)','net'));
}
function addRow(parent,lvl,label,kind,count,compId){ const r=document.createElement('button'); r.type='button'; r.className='tree-row lvl'+lvl;
  const tiIcon={folder:'<path d="M3 7a2 2 0 0 1 2-2h3l2 2h7a2 2 0 0 1 2 2v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
    doc:'<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4"/>',box:'<rect x="4" y="6" width="16" height="12" rx="1.6"/>',
    part:'<rect x="7" y="7" width="10" height="10" rx="1.4"/><path d="M9 4v3M15 4v3M9 17v3M15 17v3"/>',
    net:'<circle cx="6" cy="12" r="2.2"/><circle cx="18" cy="12" r="2.2"/><path d="M8.2 12h7.6"/>'}[kind];
  r.innerHTML='<span class="tw">'+(lvl<3?'<svg class="ico" style="width:10px;height:10px" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="m6 9 6 6 6-6"/></svg>':'')+'</span>'
    +'<span class="ti"><svg class="ico" style="width:13px;height:13px" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">'+tiIcon+'</svg></span>'
    +'<span class="tl">'+escapeHtml(label)+'</span>'+(count?'<span class="tc">'+count+'</span>':'');
  if(compId) r.onclick=()=>{ selectComp(compById(compId),false); focusComp(compId); };
  parent.appendChild(r);
}
function focusComp(id){ const c=compById(id); if(!c) return; const b=compBounds(c), host=$('#canvasHost').getBoundingClientRect();
  view.ox=host.width/2-(b.x+b.w/2)*view.scale; view.oy=host.height/2-(b.y+b.h/2)*view.scale; draw(); }

/* ------------------------------------------------------------ status */
function renderStatus(){ const s=sheet(), t=TOOLS.find(x=>x[0]===st.tool);
  $('#stTool').textContent='工具：'+(t?t[2]:'选择');
  $('#stZoom').textContent='缩放 '+Math.round(view.scale*100)+'%';
  $('#stCount').textContent='元件 '+s.comps.length+'　导线 '+s.wires.length+'　选中 '+(s.sel.size+s.wires.filter(w=>w.sel).length);
  const sim=$('#stSim'); sim.textContent=simStatus(); sim.classList.toggle('run',st.simRunning);
}

/* ------------------------------------------------------------ console */
function log(msg,cls){ const body=$('#consoleBody'); const line=document.createElement('div'); line.className='log-line'+(cls?' '+cls:'');
  const n=new Date(), ts=[n.getHours(),n.getMinutes(),n.getSeconds()].map(x=>String(x).padStart(2,'0')).join(':');
  line.innerHTML='<span class="ts">['+ts+']</span><span class="msg"></span>'; line.querySelector('.msg').textContent=msg;
  body.appendChild(line); body.scrollTop=body.scrollHeight; while(body.children.length>240) body.removeChild(body.firstChild); }

/* ------------------------------------------------------------ actions */
function markDirty(){ proj.dirty=true; $('#dirtyChip').hidden=false; updateTitle(); }
function updateTitle(){ $('#titleCrumbs').textContent=proj.name+' · '+sheet().name; }
function notImplemented(){ log('该功能尚未实现（骨架版暂只提供删除与全选）。','warn'); }
function shortcuts(){ log('快捷键：S 选择 · T 文本 · C 元件 · W 导线 · D 绘图 · E 擦除 · H 拖拽 ｜ Ctrl+Z/Y 撤销重做 ｜ F5/F6 运行停止 ｜ Ctrl+T 单步 ｜ Ctrl+Shift+T 复位 ｜ Ctrl+0 适应窗口'); }
function about(){ log('SiliFlow 0.1.0 — 数字电路原理图编辑器（类 Logisim 形态）。Apple 视觉改版原型。'); }
function act(id){
  const host=$('#canvasHost').getBoundingClientRect();
  switch(id){
    case 'rotate': case 'rotateccw': { const dir=id==='rotateccw'?-1:1, s=sheet();
      s.comps.filter(c=>s.sel.has(c.id)).forEach(c=>{ c.rot=((c.rot+dir*90)%360+360)%360; }); refreshWirePaths(); markDirty(); renderProp(); draw(); break; }
    case 'mirror': { const s=sheet(); s.comps.filter(c=>s.sel.has(c.id)).forEach(c=>c.mirror=!c.mirror); refreshWirePaths(); markDirty(); renderProp(); draw(); break; }
    case 'delete': { const s=sheet(), ids=new Set(s.comps.filter(c=>s.sel.has(c.id)).map(c=>c.id));
      if(ids.size||s.wires.some(w=>w.sel)){ s.comps=s.comps.filter(c=>!ids.has(c.id)); s.wires=s.wires.filter(w=>!w.sel&&!ids.has(w.a.c)&&!ids.has(w.b.c));
        markDirty(); evaluate(); clearSel(); log('已删除选中对象。'); } break; }
    case 'selectall': { const s=sheet(); s.comps.forEach(c=>s.sel.add(c.id)); s.wires.forEach(w=>w.sel=true); syncSelection(); break; }
    case 'zoomin': zoomAt(view.scale*1.2,{x:host.width/2,y:host.height/2}); break;
    case 'zoomout': zoomAt(view.scale/1.2,{x:host.width/2,y:host.height/2}); break;
    case 'zoomreset': zoomAt(1,{x:host.width/2,y:host.height/2}); break;
    case 'fit': zoomFit(); break;
    case 'grid': opts.showGrid=!opts.showGrid; log((opts.showGrid?'显示':'隐藏')+'网格'); draw(); break;
    case 'pinnames': opts.showPinNames=!opts.showPinNames; draw(); break;
    case 'labels': opts.showLabels=!opts.showLabels; draw(); break;
    case 'run': st.simRunning=true; evaluate(); renderToolbar(); renderStatus(); log('仿真运行，节拍 '+st.simHz+' Hz。'); draw(); break;
    case 'stop': st.simRunning=false; renderToolbar(); renderStatus(); log('仿真已停止。'); draw(); break;
    case 'step': st.tick++; evaluate(); renderStatus(); log('单步：tick '+st.tick); draw(); break;
    case 'reset': st.tick=0; sheet().comps.forEach(c=>{ if(c.type==='input') c.inState='0'; }); evaluate(); renderStatus(); renderProp(); log('仿真已复位（强制电平与 tick 计数都清空）。'); draw(); break;
    case 'undo': case 'redo': log('撤销 / 重做：原型演示未接入快照栈。','warn'); break;
    case 'new': proj.name='未命名'; proj.dirty=false; $('#dirtyChip').hidden=true; sheets.length=0; sheets.push(newSheet('main'),newSheet('sheet2'));
      activeSheet='main'; evaluate(); updateTitle(); renderAll(); log('新工程已就绪。'); break;
    case 'save': case 'saveas': proj.dirty=false; $('#dirtyChip').hidden=true; log('已保存工程（原型演示，未实际写盘）。','ok'); break;
    case 'open': log('打开工程（原型演示，未接入文件系统）。'); break;
    case 'vcd': log('打开 VCD 波形属于 M5 里程碑。','warn'); break;
    case 'exit': log('退出：原型中不实际关闭窗口。'); break;
    case 'newsheet': { let n=1; while(sheets.find(s=>s.name==='sheet'+n)) n++; const nm='sheet'+n; sheets.push(newSheet(nm)); switchSheet(nm); log('新建图纸：'+nm); break; }
    case 'closesheet': { if(sheets.length<=1){ log('至少保留一张图纸，无法关闭。','warn'); break; } const nm=activeSheet;
      sheets.splice(sheets.findIndex(s=>s.name===nm),1); switchSheet(sheets[0].name); log('已关闭图纸：'+nm); break; }
  }
  renderMenubar();
}

/* ------------------------------------------------------------ sheets / tabs */
function switchSheet(name){ activeSheet=name; refreshWirePaths(); evaluate(); renderTabs(); renderAll(); updateTitle(); }
function renderAll(){ refreshWirePaths(); renderToolbox(); renderTabs(); renderProp(); renderTree(); renderStatus(); renderZoom(); draw(); }
function renderTabs(){
  const el=$('#tabs'); el.innerHTML='';
  sheets.forEach(s=>{ const t=document.createElement('button'); t.className='tab'+(s.name===activeSheet?' active':''); t.type='button';
    t.innerHTML='<span>'+escapeHtml(s.name)+'</span><span class="tab-x" title="关闭图纸"><svg viewBox="0 0 24 24" width="11" height="11" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="M6 6l12 12M18 6 6 18"/></svg></span>';
    t.onclick=e=>{ if(e.target.closest('.tab-x')){ e.stopPropagation(); closeSheet(s.name); } else switchSheet(s.name); };
    el.appendChild(t); });
  const add=document.createElement('button'); add.className='tab-add'; add.title='新建图纸'; add.innerHTML='<svg class="ico" style="width:14px;height:14px" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"><path d="M12 5v14M5 12h14"/></svg>';
  add.onclick=()=>act('newsheet'); el.appendChild(add);
}
function closeSheet(name){ if(sheets.length<=1){ log('至少保留一张图纸，无法关闭。','warn'); return; }
  sheets.splice(sheets.findIndex(s=>s.name===name),1); if(activeSheet===name) activeSheet=sheets[0].name; refreshWirePaths(); evaluate(); renderTabs(); renderAll(); updateTitle(); log('已关闭图纸：'+name); }

/* ------------------------------------------------------------ panels + theme */
function togglePanel(side){ const narrow=matchMedia('(max-width:980px)').matches;
  const el = side==='left'?$('#panelLeft') : side==='right'?$('#panelRight') : $('#panelBottom');
  if(side==='bottom'){ el.hidden=!el.hidden; draw(); return; }
  if(narrow){ el.classList.toggle('open'); $('#scrim').classList.toggle('open', !!document.querySelector('.panel-left.open,.panel-right.open')); }
  else el.hidden=!el.hidden;
  renderMenubar(); draw();
}
$('#scrim').onclick=()=>{ $('#panelLeft').classList.remove('open'); $('#panelRight').classList.remove('open'); $('#scrim').classList.remove('open'); };
$('#panelToggleBtn').onclick=()=>{ const narrow=matchMedia('(max-width:980px)').matches; if(narrow){ togglePanel('left'); } else { togglePanel('left'); togglePanel('right'); } };
$('#themeBtn').onclick=()=>{ theme = theme==='dark'?'light':'dark'; document.documentElement.setAttribute('data-theme',theme);
  $('#themeBtn').title = theme==='dark'?'切换到浅色外观':'切换到深色外观'; draw(); log('外观：'+(theme==='dark'?'深色':'浅色')); };
$('#helpBtn').onclick=shortcuts;
$('#treeRefresh').onclick=()=>{ renderTree(); log('层次树已刷新。'); };
$('#searchInput').oninput=()=>renderToolbox();
$('#searchInput').addEventListener('keydown',e=>e.stopPropagation());
$('#fabZoomIn').onclick=()=>act('zoomin'); $('#fabZoomOut').onclick=()=>act('zoomout');
$('#fabZoomFit').onclick=()=>act('fit'); $('#fabZoomReset').onclick=()=>act('zoomreset');
$('#waveOpen').onclick=()=>log('波形查看属于 M5 里程碑：面板与布局已就位，解析与绘制尚未接入。','warn');
$('#waveClear').onclick=()=>{ $('#waveSignals').innerHTML='<div class="empty-note">（未加载）</div>'; $('#waveInfo').textContent='未加载'; log('波形已清空。'); };
$('#consoleClear').onclick=()=>{ $('#consoleBody').innerHTML=''; };

/* ------------------------------------------------------------ keyboard */
addEventListener('keydown',e=>{
  const tag=(e.target.tagName||'').toLowerCase();
  if(tag==='input'||tag==='textarea'||tag==='select') return;
  if(e.key===' '){ spaceDown=true; cv.style.cursor='grab'; e.preventDefault(); return; }
  const k=e.key.toLowerCase();
  const keyMap={s:'Select',h:'Drag',t:'Text',c:'Component',w:'Wire',d:'Drawing',e:'Eraser'};
  if(!e.ctrlKey&&!e.metaKey&&!e.altKey&&keyMap[k]){ setTool(keyMap[k]); return; }
  if(e.ctrlKey||e.metaKey){
    if(k==='z'){ e.preventDefault(); act(e.shiftKey?'redo':'undo'); }
    else if(k==='y'){ e.preventDefault(); act('redo'); }
    else if(k==='a'){ e.preventDefault(); act('selectall'); }
    else if(k==='s'){ e.preventDefault(); act(e.shiftKey?'saveas':'save'); }
    else if(k==='n'){ e.preventDefault(); act('new'); }
    else if(k==='o'){ e.preventDefault(); act('open'); }
    else if(k==='r'){ e.preventDefault(); act(e.shiftKey?'rotateccw':'rotate'); }
    else if(k==='m'){ e.preventDefault(); act('mirror'); }
    else if(k==='t'){ e.preventDefault(); act(e.shiftKey?'reset':'step'); }
    else if(k==='='||k==='+'){ e.preventDefault(); act('zoomin'); }
    else if(k==='-'){ e.preventDefault(); act('zoomout'); }
    else if(k==='0'){ e.preventDefault(); act('fit'); }
    else if(k==='1'){ e.preventDefault(); act('zoomreset'); }
    return;
  }
  if(e.key==='Delete'||e.key==='Backspace'){ act('delete'); }
  else if(e.key==='Escape'){ if(st.placing){ st.placing=null; setTool('Select'); } else if(st.wireStart){ st.wireStart=null; draw(); } else clearSel(); }
  else if(e.key==='F1'){ e.preventDefault(); shortcuts(); }
  else if(e.key==='F5'){ e.preventDefault(); act('run'); }
  else if(e.key==='F6'){ e.preventDefault(); act('stop'); }
});
addEventListener('keyup',e=>{ if(e.key===' '){ spaceDown=false; cv.style.cursor='default'; } });

/* ------------------------------------------------------------ init */
renderMenubar(); renderToolbar();
refreshWirePaths(); evaluate();
renderToolbox(); renderTabs(); renderProp(); renderTree(); renderStatus();
log('SiliFlow 已启动。');
log('用法：左栏选元件 → 画布单击放置；W 切导线工具后点两个引脚即可连线。');
log('仿真：S 工具下单击输入引脚 / 常量 / 指示灯可翻转电平；Ctrl+T 单步，Ctrl+Shift+T 复位。');
log('内置元件库：'+LIB_TOTAL+' 个元件定义。');
requestAnimationFrame(()=>{ resize(); renderZoom(); });
addEventListener('resize',resize);
