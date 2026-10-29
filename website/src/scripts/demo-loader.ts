let loaded:typeof import('./demo')|undefined,pending:Promise<typeof import('./demo')>|undefined,ready=false;
const load=()=>pending??=import('./demo').then(module=>loaded=module).catch(error=>{pending=undefined;const status=document.querySelector('#demo-status');if(status)status.textContent='The preview could not load. Please try again.';throw error});
const initialize=()=>{if(!ready&&loaded){loaded.initializeDemo();ready=true;document.documentElement.dataset.demoReady='true'}return ready};
const relevant=(target:EventTarget|null)=>target instanceof Element?target.closest<HTMLElement>('#island-demo,[data-search-open],[data-chapter]'):null;
document.addEventListener('click',event=>{
 if(ready||!relevant(event.target))return;
 if(initialize())return;
 const target=event.target instanceof Element?event.target.closest<HTMLElement>('button,a,#capsule'):null;
 if(!target)return;
 event.preventDefault();event.stopImmediatePropagation();
 void load().then(()=>{initialize();target.click()}).catch(()=>{});
},true);
document.addEventListener('keydown',event=>{
 if(ready||!['Enter',' '].includes(event.key)||!relevant(event.target))return;
 if(initialize())return;
 const target=event.target as HTMLElement;event.preventDefault();event.stopImmediatePropagation();
 void load().then(()=>{initialize();if(target.id==='capsule')target.dispatchEvent(new KeyboardEvent('keydown',{key:event.key,bubbles:true}));else target.click()}).catch(()=>{});
},true);
document.addEventListener('focusin',event=>{if(relevant(event.target))void load().then(initialize).catch(()=>{})},true);
document.addEventListener('pointerover',event=>{
 if(ready||!(event.target instanceof Element)||!event.target.closest('#capsule'))return;
 void load().then(()=>{initialize();const capsule=document.querySelector<HTMLElement>('#capsule');if(capsule?.matches(':hover'))capsule.dispatchEvent(new PointerEvent('pointerenter'))}).catch(()=>{});
},true);
const stage=document.querySelector('#island-demo');
if(stage&&'IntersectionObserver' in window){const observer=new IntersectionObserver(entries=>{if(entries.some(entry=>entry.isIntersecting)){void load().catch(()=>{});observer.disconnect()}},{rootMargin:'180px'});observer.observe(stage)}
else if(stage)void load().then(initialize).catch(()=>{});