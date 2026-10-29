import {createRequire} from 'node:module';
import {readFileSync,mkdirSync,writeFileSync,mkdtempSync} from 'node:fs';
import {join} from 'node:path';import {tmpdir} from 'node:os';
const load=createRequire(join(process.env.LIGHTHOUSE_MODULE_ROOT,'lighthouse/package.json'));
const puppeteer=load('puppeteer-core'),axe=readFileSync(join(process.env.LIGHTHOUSE_MODULE_ROOT,'axe-core/axe.min.js'),'utf8');
const out=join(tmpdir(),'Floatlet-Product-QA');mkdirSync(out,{recursive:true});const origin=process.env.FLOATLET_QA_ORIGIN||'http://127.0.0.1:4328';
const browser=await puppeteer.launch({executablePath:process.env.CHROME_PATH,headless:true,userDataDir:mkdtempSync(join(tmpdir(),'Floatlet-Product-'))});
const findings=[];const assert=(ok,message)=>{if(!ok)throw Error(message)};
try{const page=await browser.newPage();await page.setViewport({width:1440,height:1000});
for(const width of [1440,390,320]){
 await page.setViewport({width,height:width===1440?1000:844,isMobile:width<500,hasTouch:width<500});await page.goto(origin,{waitUntil:'networkidle0'});
 for(const mode of ['music','clock']){await page.click('.mode-selector [data-mode="'+mode+'"]');await page.waitForFunction(mode=>document.querySelector('#island-demo').dataset.mode===mode,{},mode);await page.mouse.move(0,0);await new Promise(resolve=>setTimeout(resolve,500));if(mode==='clock'){
  for(const clock of ['timer','alarm','stopwatch']){await page.click('[data-clock="'+clock+'"]');await page.screenshot({path:join(out,width+'-'+clock+'.png')});await page.evaluate(axe);const result=await page.evaluate(async()=>axe.run(document.querySelector('#island-demo'),{runOnly:{type:'tag',values:['wcag2a','wcag2aa']}}));findings.push({width,clock,violations:result.violations.map(v=>({id:v.id,nodes:v.nodes.map(n=>n.target)}))});}
 }else await page.screenshot({path:join(out,width+'-music.png')});}
 await page.click('[data-clock="timer"]');await page.click('#exact-time');await page.$eval('#timer-hours',el=>el.value='0');await page.$eval('#timer-minutes',el=>el.value='0');await page.$eval('#timer-seconds',el=>el.value='3');await page.click('#exact-timer-form button[type="submit"]');assert(await page.$eval('#clock-readout',el=>el.textContent)==='00:03','Exact timer did not apply');await page.click('#clock-start');await page.waitForFunction(()=>document.querySelector('#island-demo').dataset.alert==='true',{timeout:6000});await page.click('.mode-selector [data-mode="clock"]');await page.click('#dismiss-alert');assert(await page.$eval('#island-demo',el=>el.dataset.alert)==='false','Alert did not dismiss');
 for(const scene of ['music','files','clock']){await page.$eval('[data-story="'+scene+'"]',el=>el.scrollIntoView({block:'center',behavior:'instant'}));await new Promise(resolve=>setTimeout(resolve,800));assert(await page.$eval('.product-story',el=>el.dataset.scene)===scene,'Wrong scroll scene '+scene+' at '+width);await page.screenshot({path:join(out,width+'-story-'+scene+'.png')});}
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Product tour overflows '+width);
 console.log('PASS custom timer, alert dismissal and scroll scenes at '+width+'px');
}
writeFileSync(join(out,'results.json'),JSON.stringify(findings,null,2));assert(!findings.some(f=>f.violations.length),'Accessibility findings: '+JSON.stringify(findings.filter(f=>f.violations.length)));console.log('PASS all product states');
}catch(error){writeFileSync(join(out,'results.json'),JSON.stringify({findings,error:String(error)},null,2));throw error}finally{await browser.close()}