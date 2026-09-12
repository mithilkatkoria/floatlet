import {readFileSync} from 'node:fs';
const r=JSON.parse(readFileSync(new URL('../src/data/release.json',import.meta.url),'utf8').replace(/^\uFEFF/,''));
if(!r||r.architecture!=='x64'||r.channel!=='preview'||!r.url.startsWith('https://github.com/mithilkatkoria/floatlet/releases/download/')||!Number.isInteger(r.size)||r.size<=0||!/^[a-f0-9]{64}$/.test(r.sha256))throw Error('Invalid release manifest');
if(process.env.SITE_ORIGIN&&!/^https:\/\/[^/]+$/.test(process.env.SITE_ORIGIN))throw Error('SITE_ORIGIN must be an HTTPS origin without a trailing slash');
if(process.env.PUBLIC_TRACKING_ENABLED==='true')throw Error('Tracking blocked until vendor IDs, network behavior and final privacy notice are verified.');
console.log('Release checks passed. Optional tracking remains disabled.');
