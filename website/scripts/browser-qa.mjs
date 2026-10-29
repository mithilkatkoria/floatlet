import {createRequire} from 'node:module';
import {readFileSync,writeFileSync,mkdirSync,mkdtempSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {tmpdir} from 'node:os';
const dependencies=process.env.LIGHTHOUSE_MODULE_ROOT;
if(!dependencies)throw Error('Set LIGHTHOUSE_MODULE_ROOT to the installed audit tools');
const loadPackage=createRequire(join(dependencies,'lighthouse/package.json'));
const puppeteer=loadPackage('puppeteer-core');
const axe=readFileSync(join(dependencies,'axe-core/axe.min.js'),'utf8');
const origin=process.env.FLOATLET_QA_ORIGIN||'http://127.0.0.1:4328';
const out=resolve(process.env.FLOATLET_QA_OUTPUT||'audit/redesign-ui');mkdirSync(out,{recursive:true});
const pages=['/','/download/','/dynamic-island-for-windows/',...JSON.parse(readFileSync('src/data/pages.json','utf8')).map(page=>'/'+page.slug+'/')];
const browser=await puppeteer.launch({executablePath:process.env.CHROME_PATH,headless:true,userDataDir:mkdtempSync(join(tmpdir(),'Floatlet-UI-')),args:['--disable-gpu','--no-first-run','--no-default-browser-check']});
const results=[],errors=[];
const assert=(condition,message)=>{if(!condition)throw Error(message)};
try{
 const page=await browser.newPage();
 page.on('pageerror',error=>errors.push(String(error)));
 page.on('console',message=>{if(message.type()==='error')errors.push(message.text())});
 await page.setViewport({width:1440,height:1000,deviceScaleFactor:1});
 await page.goto(origin+'/',{waitUntil:'networkidle0'});
 assert(await page.evaluate(()=>!document.documentElement.dataset.demoReady),'Playground initialized before intent');
 await page.click('.mode-selector [data-mode="calendar"]');
 await page.waitForFunction(()=>document.querySelector('#island-demo').dataset.mode==='calendar');
 assert(await page.$$eval('#calendar-grid button',items=>items.length)>=28,'First click did not initialize calendar');
 await page.click('[data-search-open]');
 await page.waitForFunction(()=>document.querySelector('#search-dialog').open);
 await page.type('#search-sample-input','Google');
 assert(await page.$eval('#search-sample-input',input=>document.activeElement===input),'Search did not receive focus');
 await page.keyboard.down('Control');await page.keyboard.press('KeyA');await page.keyboard.up('Control');await page.keyboard.type('Notepad');
 assert(await page.$eval('#search-sample-input',input=>input.value)==='Notepad','Search select-all failed');
 await page.keyboard.press('Escape');
 assert(!(await page.$eval('#search-dialog',dialog=>dialog.open)),'Escape did not close search');
 for(const mode of ['music','files','clock','controls','settings','call']){
  await page.click('.mode-selector [data-mode="'+mode+'"]');await page.waitForFunction(mode=>document.querySelector('#island-demo').dataset.mode===mode,{},mode);
 }
 await page.click('.mode-selector [data-mode="clock"]');await page.click('#clock-start');
 await page.waitForFunction(()=>document.querySelector('#clock-start').textContent==='Pause');
 await page.click('#clock-start');await page.click('#clock-reset');
 await page.goto(origin+'/',{waitUntil:'networkidle0'});
 const firstTheme=await page.$eval('html',html=>html.dataset.theme);
 await page.click('#theme-lamp');await page.waitForFunction(theme=>document.documentElement.dataset.theme!==theme,{},firstTheme);
 await page.waitForFunction(()=>!document.documentElement.classList.contains('theme-transition'));
 const chosen=await page.$eval('html',html=>html.dataset.theme);await page.reload({waitUntil:'networkidle0'});
 assert(await page.$eval('html',html=>html.dataset.theme)===chosen,'Theme did not persist');
 const bead=await page.$('.lamp-handle');const rect=await bead.boundingBox();
 await page.mouse.move(rect.x+rect.width/2,rect.y+rect.height/2);await page.mouse.down();await page.mouse.move(rect.x+rect.width/2+8,rect.y+rect.height/2+27,{steps:8});await page.mouse.up();
 await page.waitForFunction(theme=>document.documentElement.dataset.theme!==theme,{},chosen);
 await page.waitForFunction(()=>!document.documentElement.classList.contains('theme-transition'));
 await page.focus('#theme-lamp');const beforeKey=await page.$eval('html',html=>html.dataset.theme);await page.keyboard.press('Enter');
 await page.waitForFunction(theme=>document.documentElement.dataset.theme!==theme,{},beforeKey);
 await page.waitForFunction(()=>!document.documentElement.classList.contains('theme-transition'));
 console.log('PASS: first click, every demo mode, Search focus/select-all, timer, lamp click/drag/keyboard and persistence');
 for(const viewport of [{name:'desktop',width:1440,height:1000},{name:'mobile',width:390,height:844,isMobile:true,hasTouch:true}]){
  await page.setViewport({...viewport,deviceScaleFactor:1});
  for(const theme of ['light','dark']){
   await page.evaluate(theme=>localStorage.setItem('floatlet-theme',theme),theme);
   for(const path of pages){
    const response=await page.goto(origin+path,{waitUntil:'networkidle0'});assert([200,304].includes(response.status()),path+' returned HTTP '+response.status());
    assert(await page.$eval('html',html=>html.dataset.theme)===theme,'Wrong theme on '+path);
    const overflow=await page.evaluate(()=>document.documentElement.scrollWidth-document.documentElement.clientWidth);assert(overflow<=1,'Horizontal overflow '+overflow+'px: '+path+' '+viewport.name+' '+theme);
    assert(await page.$eval('#theme-lamp',button=>button.getAttribute('aria-checked'))===String(theme==='dark'),'Lamp state mismatch');
    await page.evaluate(axe);
    const access=await page.evaluate(async()=>await axe.run(document,{runOnly:{type:'tag',values:['wcag2a','wcag2aa','wcag21aa','wcag22aa']}}));
    const violations=access.violations.map(item=>({id:item.id,impact:item.impact,nodes:item.nodes.map(node=>node.target)}));
    results.push({path,viewport:viewport.name,theme,overflow,violations});
    const key=(path==='/'?'home':path.replace(/^\/|\/$/g,'').replaceAll('/','-'))+'-'+viewport.name+'-'+theme;
    await page.screenshot({path:join(out,key+'.png')});
    console.log(JSON.stringify({path,viewport:viewport.name,theme,violations:violations.map(item=>item.id)}));
   }
  }
 }
 await page.setViewport({width:320,height:740,isMobile:true,hasTouch:true,deviceScaleFactor:1});
 await page.goto(origin+'/',{waitUntil:'networkidle0'});
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=document.documentElement.clientWidth+1),'320px layout overflow');
 await page.screenshot({path:join(out,'home-320.png')});
 await page.emulateMediaFeatures([{name:'prefers-reduced-motion',value:'reduce'}]);await page.click('#theme-lamp');
 assert(await page.evaluate(()=>!document.documentElement.classList.contains('theme-transition')),'Reduced motion used a theme transition');
 await page.evaluate(()=>localStorage.removeItem('floatlet-theme'));await page.emulateMediaFeatures([{name:'prefers-color-scheme',value:'dark'}]);await page.reload({waitUntil:'networkidle0'});
 assert(await page.$eval('html',html=>html.dataset.theme)==='dark','System theme fallback failed');
 assert(errors.length===0,'Browser errors: '+errors.join('\n'));
 writeFileSync(join(out,'results.json'),JSON.stringify({checks:results,errors},null,2));
 const violations=results.flatMap(result=>result.violations);assert(!violations.length,violations.length+' accessibility violations; see results.json');
 console.log('PASS: '+results.length+' page/viewport/theme checks, 320px layout, reduced motion, system theme and no browser errors');
}catch(error){writeFileSync(join(out,'results.json'),JSON.stringify({checks:results,errors,failure:String(error)},null,2));throw error}
finally{await browser.close()}