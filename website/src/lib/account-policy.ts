// UI feedback only. Supabase must enforce the actual password policy server-side.
export function passwordScore(value:string):number{
  if(value.length<8||/^(.)\1+$/u.test(value))return 0;
  const words=value.toLowerCase().trim().split(/\s+/);
  if(words.length>1&&new Set(words).size===1)return 0;
  if(/^(password|qwerty|letmein|welcome|123456)[\d\W_]*$/i.test(value))return 0;
  const groups=[/[a-z]/,/[A-Z]/,/\d/,/[^A-Za-z\d]/].filter(pattern=>pattern.test(value)).length;
  if(value.length>=20&&new Set(words).size>=3)return 4;
  if(value.length>=16&&groups>=2)return groups>=3?4:3;
  if(value.length>=12)return groups>=3?groups:2;
  return 1;
}
export function passwordAllowed(value:string):boolean{
  return [...value].length>=15&&passwordScore(value)>0;
}
