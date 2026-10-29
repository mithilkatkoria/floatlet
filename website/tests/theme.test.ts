import test from 'node:test';
import assert from 'node:assert/strict';
import {runInNewContext} from 'node:vm';
import {createHash} from 'node:crypto';
import {readFileSync} from 'node:fs';
import {themeBootstrap} from '../src/lib/theme-bootstrap.ts';
function boot(saved:string|null,systemDark=false,blocked=false){
 const document={querySelector:()=>null,documentElement:{dataset:{} as Record<string,string>,style:{} as Record<string,string>}};
 runInNewContext(themeBootstrap,{document,localStorage:{getItem(){if(blocked)throw Error('storage blocked');return saved}},matchMedia:()=>({matches:systemDark})});
 return document.documentElement;
}
test('every page opens light regardless of saved theme, system preference or blocked storage',()=>{
 for(const saved of ['dark','light','invalid',null]){
  for(const systemDark of [false,true]){
   for(const blocked of [false,true]){
    const root=boot(saved,systemDark,blocked);
    assert.equal(root.dataset.theme,'light');
    assert.equal(root.style.colorScheme,'light');
   }
  }
 }
});
test('the first-paint theme script is authorized by the exact production CSP hash',()=>{
 const config=JSON.parse(readFileSync(new URL('../vercel.json',import.meta.url),'utf8'));
 const csp=config.headers.find((entry:{source:string})=>entry.source==='/(.*)').headers.find((entry:{key:string})=>entry.key==='Content-Security-Policy').value;
 const hash=createHash('sha256').update(themeBootstrap).digest('base64');
 assert.ok(csp.includes("'sha256-"+hash+"'"));
 assert.ok(!csp.includes("script-src 'self' 'unsafe-inline'"));
});