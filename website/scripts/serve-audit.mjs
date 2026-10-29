import {createServer} from 'node:http';
import {readFileSync,existsSync,statSync} from 'node:fs';
import {resolve,join,sep,extname} from 'node:path';
const base=resolve('dist'),headers=JSON.parse(readFileSync('vercel.json','utf8')).headers[0].headers;
const types={'.html':'text/html; charset=utf-8','.css':'text/css','.js':'text/javascript','.svg':'image/svg+xml','.png':'image/png','.txt':'text/plain; charset=utf-8','.xml':'application/xml','.woff2':'font/woff2'};
createServer((req,res)=>{
 let route;try{route=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname)}catch{res.writeHead(400);res.end();return}
 let file=resolve(base,'.'+route);if(file!==base&&!file.startsWith(base+sep)){res.writeHead(403);res.end();return}
 if(existsSync(file)&&statSync(file).isDirectory())file=join(file,'index.html');
 const found=existsSync(file)&&statSync(file).isFile();if(!found)file=join(base,'404.html');
 for(const header of headers)res.setHeader(header.key,header.value);
 res.setHeader('Content-Type',types[extname(file)]||'application/octet-stream');res.writeHead(found?200:404);res.end(readFileSync(file));
}).listen(4328,'127.0.0.1',()=>console.log('Floatlet production preview: http://127.0.0.1:4328'));