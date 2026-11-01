import {readFileSync} from 'node:fs';
const r=JSON.parse(readFileSync(new URL('../src/data/release.json',import.meta.url),'utf8').replace(/^\uFEFF/,''));
if(!r||r.architecture!=='x64'||r.channel!=='preview'||!r.url.startsWith('https://github.com/mithilkatkoria/floatlet/releases/download/')||!Number.isInteger(r.size)||r.size<=0||!/^[a-f0-9]{64}$/.test(r.sha256))throw Error('Invalid release manifest');
if(process.env.SITE_ORIGIN&&!/^https:\/\/[^/]+$/.test(process.env.SITE_ORIGIN))throw Error('SITE_ORIGIN must be an HTTPS origin without a trailing slash');
if(process.env.PUBLIC_TRACKING_ENABLED==='true'){if(!/^G-[A-Z0-9]+$/.test(process.env.PUBLIC_GA4_ID||''))throw Error('A verified GA4 measurement ID is required');if(process.env.PUBLIC_CLARITY_ID)throw Error('Session replay is not enabled for this analytics release');}
console.log('Release and analytics configuration checks passed.');
