export type Evidence = 'confirmed' | 'unsupported' | 'not-tested';
export interface Release { version:string; date:string; architecture:'x64'; windows:string; url:string; size:number; sha256:string; signing:'unsigned'; notes:string; channel:'preview'; }
export const product = { name:'Floatlet', description:'Music, files and useful controls, gathered into one compact companion for Windows.', repository:'https://github.com/mithilkatkoria/floatlet', support:'https://github.com/mithilkatkoria/floatlet/issues', author:'Mithil Katkoria', origin:import.meta.env.SITE_ORIGIN || 'https://floatlet.vercel.app', mode:import.meta.env.SITE_MODE || 'production', consentVersion:'1', legalApproved:false, mediaApproved:false };
export const features: {name:string;status:Evidence;evidence:string}[] = [
{name:'Music and hover transport',status:'confirmed',evidence:'src/app/main.cpp; src/services/media.cpp'},
{name:'File references and outgoing copy drag',status:'confirmed',evidence:'src/platform/shelf.cpp; integration tests'},
{name:'Volume and supported internal brightness',status:'confirmed',evidence:'src/services/audio.h; src/services/system_controls.h'},
{name:'Timer and calendar subscription',status:'confirmed',evidence:'src/ui/countdown.h; src/services/calendar_feed.h; synthetic calendar tests'},
{name:'Native ARM64 release',status:'unsupported',evidence:'Only x64 binary supplied'},
{name:'Caller identity and answer/end controls',status:'unsupported',evidence:'Microphone metadata only'},
{name:'Battery savings',status:'not-tested',evidence:'No battery experiment'},
];
export const screenshots: {src:string;alt:string;width:number;height:number}[] = [];
