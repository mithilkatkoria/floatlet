import {createClient} from '@supabase/supabase-js';
interface Signup{id:string;email:string|null;createdAt:string;confirmedAt:string|null;lastSignIn:string|null;providers:string[]}
const root=document.querySelector<HTMLElement>('#admin-app');
if(root){
  const find=<T extends HTMLElement>(selector:string)=>root.querySelector<T>(selector)!;
  const status=find<HTMLElement>('#admin-status'),data=find<HTMLElement>('#admin-data'),signin=find<HTMLAnchorElement>('#admin-signin');
  const prev=find<HTMLButtonElement>('#admin-prev'),next=find<HTMLButtonElement>('#admin-next'),refresh=find<HTMLButtonElement>('#admin-refresh');
  let page=1,busy=false;
  const url=root.dataset.url||'',key=root.dataset.key||'';
  if(!url||!key){status.textContent='The account service is not connected yet.';refresh.disabled=true;}
  else{
    const client=createClient(url,key,{auth:{flowType:'pkce',autoRefreshToken:true,persistSession:true}});
    const format=(value:string|null)=>value?new Date(value).toLocaleString(undefined,{dateStyle:'medium',timeStyle:'short'}):'N/A';
    const load=async()=>{
      if(busy)return;busy=true;refresh.disabled=prev.disabled=next.disabled=true;data.hidden=true;signin.hidden=true;status.textContent='Loading your signup dashboard…';
      try{
        const {data:sessionData}=await client.auth.getSession();
        if(!sessionData.session){status.textContent='Sign in with the account authorized to manage Floatlet.';signin.hidden=false;return;}
        const response=await fetch(url+'/functions/v1/admin-signups?page='+page,{headers:{apikey:key,Authorization:'Bearer '+sessionData.session.access_token},cache:'no-store'});
        const body=await response.json();
        if(!response.ok){status.textContent=body.error||'The signup list could not be loaded.';signin.hidden=response.status!==401&&response.status!==403;return;}
        const users=body.users as Signup[],rows=find<HTMLElement>('#signup-rows');rows.replaceChildren();
        for(const user of users){const row=document.createElement('tr');for(const value of [user.email||'No email',format(user.createdAt),user.confirmedAt?'Confirmed':'Awaiting confirmation',user.providers.join(', ')||'Email',format(user.lastSignIn)]){const cell=document.createElement('td');cell.textContent=value;row.append(cell);}rows.append(row);}
        if(!users.length){const row=document.createElement('tr'),cell=document.createElement('td');cell.colSpan=5;cell.textContent='Your first signups will appear here.';cell.className='admin-empty';row.append(cell);rows.append(row);}
        find<HTMLElement>('#admin-total').textContent=String(body.total??users.length);
        find<HTMLElement>('#admin-total-label').textContent=body.total===null?'Accounts on this page':'Across Floatlet';
        find<HTMLElement>('#admin-confirmed').textContent=String(users.filter(user=>user.confirmedAt).length);
        find<HTMLElement>('#admin-page').textContent='Page '+page;
        status.textContent='Updated '+new Date().toLocaleTimeString(undefined,{hour:'2-digit',minute:'2-digit'})+'.';
        data.hidden=false;prev.disabled=page===1;next.disabled=!body.hasNext;
      }catch{status.textContent='The dashboard could not be loaded. Check your connection and try again.';}
      finally{busy=false;refresh.disabled=false;}
    };
    refresh.addEventListener('click',()=>void load());
    prev.addEventListener('click',()=>{if(page>1&&!busy){--page;void load();}});
    next.addEventListener('click',()=>{if(!busy){++page;void load();}});
    client.auth.onAuthStateChange(event=>{if(event==='SIGNED_OUT'){data.hidden=true;find<HTMLElement>('#signup-rows').replaceChildren();status.textContent='You have signed out.';signin.hidden=false;}});
    void load();
  }
}
