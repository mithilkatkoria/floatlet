export type Mode='idle'|'peek'|'music'|'files'|'controls'|'clock'|'calendar'|'call'|'settings';
export type DemoState={mode:Mode;pinned:boolean;files:string[];track:number;playing:boolean};
export const initial=():DemoState=>({mode:'idle',pinned:false,files:[],track:0,playing:false});
export function transition(s:DemoState,event:string,value?:Mode):DemoState {if(event==='reset')return initial();if(event==='escape')return {...s,mode:'idle',pinned:false};if(event==='leave')return s.pinned?s:{...s,mode:'idle'};if(event==='hover')return s.mode==='idle'?{...s,mode:'peek'}:s;if(event==='mode'&&value)return {...s,mode:value};if(event==='pin')return {...s,pinned:!s.pinned};return s}
