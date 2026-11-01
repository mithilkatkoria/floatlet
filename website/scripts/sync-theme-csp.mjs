import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const source=readFileSync(new URL('../src/lib/theme-bootstrap.ts',import.meta.url),'utf8');
const match=source.match(/export const themeBootstrap=("(?:[^"\\]|\\.)*");/);
if(!match)throw Error('Theme bootstrap must remain a literal string');
const hash=createHash('sha256').update(JSON.parse(match[1])).digest('base64');
const file=new URL('../vercel.json',import.meta.url),config=JSON.parse(readFileSync(file,'utf8'));
const header=config.headers.find(item=>item.source==='/(.*)').headers.find(item=>item.key==='Content-Security-Policy');
const directive="script-src 'self' 'sha256-"+hash+"' https://www.googletagmanager.com";
const expected=header.value.replace(/script-src[^;]+/,directive);
if(process.argv.includes('--check')){if(header.value!==expected)throw Error('Run npm run theme:csp before building');console.log('Theme bootstrap CSP verified.');}
else{header.value=expected;writeFileSync(file,JSON.stringify(config,null,2)+'\n');console.log('Theme bootstrap CSP updated.');}