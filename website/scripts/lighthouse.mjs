import {readFileSync,writeFileSync,mkdirSync,existsSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';
const args=process.argv.slice(2);
const option=(name,fallback)=>args.find(value=>value.startsWith(name+'='))?.slice(name.length+1)||fallback;
const origin=option('--origin','https://floatlet.vercel.app').replace(/\/$/,'');
const directory=resolve(option('--out','audit/pages'));
const modules=process.env.LIGHTHOUSE_MODULE_ROOT;
const moduleUrl=path=>modules?pathToFileURL(join(modules,path)).href:path.split('/')[0];
const {default:lighthouse}=await import(moduleUrl('lighthouse/core/index.js'));
const {launch}=await import(moduleUrl('chrome-launcher/dist/index.js'));
const pages=JSON.parse(readFileSync(new URL('../src/data/pages.json',import.meta.url),'utf8'));
let paths=['/','/download/','/dynamic-island-for-windows/',...pages.map(page=>'/'+page.slug+'/')];
if(args.includes('--private'))paths.push('/account/','/admin/','/404.html');
const selected=option('--paths','');if(selected)paths=selected.split(',');
const devices=option('--devices','mobile,desktop').split(',');
const cases=option('--cases','').split(',').filter(Boolean);
const requestedRuns=Number(option('--runs','1'));
if(!Number.isInteger(requestedRuns)||requestedRuns<1||requestedRuns>5)throw Error('--runs must be between 1 and 5');
mkdirSync(directory,{recursive:true});
const summaryFile=join(directory,'summary.json');
const results=args.includes('--resume')&&existsSync(summaryFile)?JSON.parse(readFileSync(summaryFile,'utf8')):[];
mkdirSync(join(directory,'browser-profile'),{recursive:true});
const browser=await launch({chromePath:process.env.CHROME_PATH,chromeFlags:['--headless=new','--disable-gpu','--no-first-run','--no-default-browser-check'],userDataDir:process.env.LIGHTHOUSE_PROFILE||join(directory,'browser-profile')});
try{
  for(const path of paths)for(const device of devices)for(let attempt=1;attempt<=requestedRuns;++attempt){
    if(cases.length&&!cases.includes(path+':'+device))continue;
    if(results.some(result=>result.path===path&&result.device===device&&(result.attempt||1)===attempt&&!result.error))continue;
    const key=(path==='/'?'home':path.replace(/^\//,'').replace(/\/$/,'').replaceAll('/','-'))+'-'+device+(requestedRuns>1?'-run'+attempt:'');
    const url=origin+path;
    try{
      const flags={port:browser.port,logLevel:'error',output:['json','html'],onlyCategories:['performance','accessibility','best-practices','seo'],maxWaitForLoad:35000};
      if(device==='desktop')Object.assign(flags,{formFactor:'desktop',screenEmulation:{mobile:false,width:1350,height:940,deviceScaleFactor:1,disabled:false},throttling:{rttMs:40,throughputKbps:10240,cpuSlowdownMultiplier:1}});
      const run=await lighthouse(url,flags);
      if(!run)throw Error('No Lighthouse report returned');
      const {lhr,report}=run;
      writeFileSync(join(directory,key+'.json'),report[0]);writeFileSync(join(directory,key+'.html'),report[1]);
      const scores=Object.fromEntries(Object.entries(lhr.categories).map(([name,category])=>[name,category.score===null?null:Math.round(category.score*100)]));
      const failures=Object.values(lhr.audits).filter(audit=>audit.score!==null&&audit.score<1&&!['manual','informative','notApplicable'].includes(audit.scoreDisplayMode)).map(({id,title,score,displayValue,details})=>({id,title,score,displayValue,items:details?.items}));
      const result={path,device,attempt,url,date:lhr.fetchTime,version:lhr.lighthouseVersion,scores,error:lhr.runtimeError||null,warnings:lhr.runWarnings,failures};
      const previous=results.findIndex(item=>item.path===path&&item.device===device&&(item.attempt||1)===attempt);if(previous>=0)results[previous]=result;else results.push(result);
      writeFileSync(summaryFile,JSON.stringify(results,null,2));
      console.log(JSON.stringify({path,device,attempt,scores,error:result.error,failed:failures.map(item=>item.id)}));
    }catch(error){results.push({path,device,url,error:String(error)});writeFileSync(summaryFile,JSON.stringify(results,null,2));console.error(path,device,String(error));}
  }
}finally{try{await browser.kill();}catch(error){console.error('Browser cleanup:',String(error));}}