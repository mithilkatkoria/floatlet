import { readFileSync, readdirSync, existsSync, statSync } from 'node:fs';
import { join, resolve } from 'node:path';
const root=resolve('dist');
let count=0;
function scan(dir){for(const name of readdirSync(dir)){const path=join(dir,name);if(statSync(path).isDirectory()){scan(path);continue;}if(!name.endsWith('.html'))continue;const html=readFileSync(path,'utf8');if(html.includes('\u2014'))throw Error('Em dash in '+path);for(const match of html.matchAll(/(?:href|src)="(\/[^"#?]*)(?:[?#][^"]*)?"/g)){const target=join(root,decodeURIComponent(match[1]));if(!existsSync(target)&&!existsSync(join(target,'index.html')))throw Error('Broken local link '+match[1]+' in '+path);count++;}}}
scan(root);console.log(`${count} local links and assets verified.`);
