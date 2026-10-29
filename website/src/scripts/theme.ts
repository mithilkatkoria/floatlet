const lamp=document.querySelector<HTMLButtonElement>('#theme-lamp');
if(lamp){
 const root=document.documentElement,reduce=matchMedia('(prefers-reduced-motion: reduce)');
 const cord=lamp.querySelector<SVGPathElement>('.lamp-cord')!,shadow=lamp.querySelector<SVGPathElement>('.lamp-cord-shadow')!,handle=lamp.querySelector<SVGGElement>('.lamp-handle')!,head=lamp.querySelector<SVGGElement>('.lamp-head')!;
 let frame=0,x=0,y=0,vx=0,vy=0,dragging=false,startX=0,startY=0,moved=false,suppressClick=false,transitioning=false;
 const sync=()=>{lamp.setAttribute('aria-checked',String(root.dataset.theme==='dark'));document.querySelector('meta[name=theme-color]')?.setAttribute('content',root.dataset.theme==='dark'?'#101816':'#f7f6f1')};
 const draw=()=>{const path='M103 90Q'+(103+x*.22)+' '+(128+y*.4)+' '+(103+x)+' '+(165+y);cord.setAttribute('d',path);shadow.setAttribute('d',path);handle.setAttribute('transform','translate('+(103+x)+' '+(165+y)+')');head.style.transform='rotate('+Math.max(-2.5,Math.min(2.5,x*.09-y*.025))+'deg)';};
 const settle=()=>{cancelAnimationFrame(frame);if(reduce.matches){x=y=vx=vy=0;draw();return}let previous=performance.now(),end=previous+950;const step=(now:number)=>{const dt=Math.min(2,(now-previous)/16.67);previous=now;vx=(vx-x*.16*dt)*Math.pow(.76,dt);vy=(vy-y*.19*dt)*Math.pow(.77,dt);x+=vx*dt;y+=vy*dt;draw();if(now<end&&(Math.abs(x)+Math.abs(y)+Math.abs(vx)+Math.abs(vy)>.12))frame=requestAnimationFrame(step);else{x=y=vx=vy=0;draw()}};frame=requestAnimationFrame(step)};
 const apply=(theme:string)=>{root.dataset.theme=theme;root.style.colorScheme=theme;sync()};
 const toggle=()=>{
  if(transitioning)return;
  const next=root.dataset.theme==='dark'?'light':'dark',bounds=lamp.getBoundingClientRect(),cx=bounds.left+bounds.width*.43,cy=bounds.top+bounds.height*.44;
  if(reduce.matches||!document.startViewTransition){apply(next);return}
  transitioning=true;root.classList.add('theme-transition');
  const change=document.startViewTransition(()=>apply(next));
  void change.ready.then(()=>{const radius=Math.hypot(Math.max(cx,innerWidth-cx),Math.max(cy,innerHeight-cy));root.animate({clipPath:['circle(0px at '+cx+'px '+cy+'px)','circle('+radius+'px at '+cx+'px '+cy+'px)']},{duration:550,easing:'cubic-bezier(.22,1,.36,1)',pseudoElement:'::view-transition-new(root)'})}).catch(()=>{});
  void change.finished.catch(()=>{}).finally(()=>{transitioning=false;root.classList.remove('theme-transition')});
 };
 lamp.addEventListener('pointerdown',event=>{if(event.button!==0)return;cancelAnimationFrame(frame);dragging=true;moved=false;startX=event.clientX;startY=event.clientY;lamp.setPointerCapture(event.pointerId);lamp.classList.add('pulling')});
 lamp.addEventListener('pointermove',event=>{if(!dragging)return;x=Math.max(-28,Math.min(28,(event.clientX-startX)*.7));y=Math.max(-6,Math.min(40,event.clientY-startY));moved=Math.abs(x)+Math.abs(y)>6;draw()});
 const release=(event:PointerEvent,cancelled=false)=>{if(!dragging)return;dragging=false;lamp.classList.remove('pulling');if(lamp.hasPointerCapture(event.pointerId))lamp.releasePointerCapture(event.pointerId);if(moved){suppressClick=true;setTimeout(()=>suppressClick=false,0);if(!cancelled&&y>9)toggle()}settle()};
 lamp.addEventListener('pointerup',event=>release(event));lamp.addEventListener('pointercancel',event=>release(event,true));
 lamp.addEventListener('click',()=>{if(suppressClick)return;if(!reduce.matches){y=15;vy=1;draw();settle()}toggle()});
 document.addEventListener('visibilitychange',()=>{if(document.hidden){cancelAnimationFrame(frame);dragging=false;x=y=vx=vy=0;draw();lamp.classList.remove('pulling')}});
 sync();
}
