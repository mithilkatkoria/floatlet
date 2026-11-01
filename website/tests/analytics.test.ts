import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import ts from 'typescript';
import { cleanEvent, denied, readConsent } from '../src/lib/policy.ts';

function harness(options: {saved?: string; path?: string; origin?: string; blockedStorage?: boolean; configured?: boolean} = {}) {
  const nodes = new Map<string, any>();
  class Element {
    hidden = true; checked = false; disabled = false; textContent = ''; open = false;
    dataset: Record<string,string> = {}; listeners: Record<string, Function> = {};
    addEventListener(name: string, fn: Function) { this.listeners[name] = fn; }
    showModal() { this.open = true; } close() { this.open = false; }
    closest() { return this; }
    click() { this.listeners.click?.({preventDefault(){}}); }
  }
  const node = (id: string) => { if (!nodes.has(id)) nodes.set(id, new Element()); return nodes.get(id); };
  const scripts: any[] = []; const listeners: Record<string, Function> = {};
  const writes: string[] = []; const cookies: string[] = []; let reloads = 0;
  const document = {
    title: 'Download Floatlet', referrer:'https://example.org/private/person?email=secret@example.org',
    querySelector: node, createElement: () => ({}), head: {append: (s: any) => scripts.push(s)},
    addEventListener: (name: string, fn: Function) => { listeners[name] = fn; },
    get cookie() { return '_ga=123; _ga_9S33BD7F7C=456; theme=light'; },
    set cookie(value: string) { cookies.push(value); },
  };
  const location = {origin: options.origin || 'https://floatlet.vercel.app', pathname:options.path || '/download/', search:'?email=secret@example.org', hash:'#private', reload(){ reloads++; }};
  const window: any = {};
  let source = readFileSync(new URL('../src/scripts/privacy.ts', import.meta.url), 'utf8').replace(/^import .*;\r?\n/gm, '');
  const env = {PUBLIC_GA4_ID: 'G-9S33BD7F7C', PUBLIC_TRACKING_ENABLED:options.configured === false ? 'false':'true', MODE:'production', SITE_ORIGIN:'https://floatlet.vercel.app'};
  source = source.replace(/import\.meta\.env\.([A-Z_0-9]+)/g, (_, key) => JSON.stringify(env[key as keyof typeof env] || ''));
  const code = ts.transpileModule(source, {compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.None}}).outputText;
  const storage = {getItem(){ if(options.blockedStorage) throw Error('unavailable'); return options.saved || null; },setItem(_k: string,v:string){if(options.blockedStorage) throw Error('unavailable');writes.push(v);}};
  vm.runInNewContext(code, {document,window,location,localStorage:storage,Element,URL,readConsent,denied,cleanEvent,release:{version:'0.8.0'}});
  return {node,scripts,window,writes,cookies,listeners,Element,reloads:()=>reloads,calls:()=>window.dataLayer?.map((a:any)=>Array.from(a)) || []};
}
test('no Google script or events before opt-in or after rejection',()=>{
  const h=harness(); assert.equal(h.scripts.length,0);assert.equal(h.calls().length,0);assert.equal(h.node('#privacy-banner').hidden,false);
  h.node('#privacy-banner-reject').click();assert.equal(h.scripts.length,0);assert.equal(h.node('#privacy-banner').hidden,true);
  assert.equal(JSON.parse(h.writes[0]).analytics,false);
});
test('opt-in queues exactly one sanitized page view, with ads denied',()=>{
  const h=harness();h.node('#privacy-banner-accept').click();h.node('#privacy-banner-accept').click();
  assert.equal(h.scripts.length,1);assert.match(h.scripts[0].src,/G-9S33BD7F7C$/);
  const calls=h.calls();const views=calls.filter((a:any)=>a[0]==='event'&&a[1]==='page_view');assert.equal(views.length,1);
  assert.equal(views[0][2].page_location,'https://floatlet.vercel.app/download/');assert.equal(views[0][2].page_referrer,'https://example.org/');
  assert.equal(calls[0][2].ad_storage,'denied');assert.equal(calls[0][2].ad_personalization,'denied');
  assert.equal(JSON.stringify(calls).includes('secret@'),false);
});
test('return visits respect stored acceptance, rejection, expiry and policy version',()=>{
  assert.equal(harness({saved:JSON.stringify({...denied(),analytics:true})}).scripts.length,1);
  assert.equal(harness({saved:JSON.stringify(denied())}).scripts.length,0);
  assert.equal(harness({saved:JSON.stringify({...denied(),analytics:true,at:0})}).scripts.length,0);
  assert.equal(harness({saved:JSON.stringify({...denied(),analytics:true,version:'1'})}).scripts.length,0);
});
test('withdrawal disables GA before reload and removes only analytics cookies',()=>{
  const h=harness({saved:JSON.stringify({...denied(),analytics:true})});h.node('#privacy-reject').click();
  assert.equal(h.window['ga-disable-G-9S33BD7F7C'],true);assert.equal(h.reloads(),1);
  assert.ok(h.cookies.some(c=>c.startsWith('_ga=')));assert.ok(!h.cookies.some(c=>c.startsWith('theme=')));
  assert.equal(JSON.parse(h.writes[0]).analytics,false);
});
test('preview hosts, private routes and unconfigured builds never collect',()=>{
  for(const options of [{origin:'https://preview.vercel.app'},{path:'/account/'},{path:'/admin/'},{configured:false}]){
    const h=harness({...options,saved:JSON.stringify({...denied(),analytics:true})});h.node('#privacy-banner-accept').click();assert.equal(h.scripts.length,0);
  }
});
test('blocked storage does not break consent or downloads',()=>{
  const h=harness({blockedStorage:true});h.node('#privacy-banner-accept').click();assert.equal(h.scripts.length,1);
});
test('download events distinguish packages and exclude arbitrary payloads',()=>{
  const h=harness({saved:JSON.stringify({...denied(),analytics:true})});const target=new h.Element();target.dataset={event:'download_click',placement:'download',packageType:'zip'};
  h.listeners.click({target});const last=h.calls().at(-1);assert.equal(last[1],'download_click');assert.equal(last[2].package_type,'zip');
  assert.equal(cleanEvent('__proto__',{}),null);assert.equal(cleanEvent('constructor',{}),null);
  assert.deepEqual(cleanEvent('download_page_open',{placement:'hero',email:'private'}),{name:'download_page_open',params:{placement:'hero'}});
});
