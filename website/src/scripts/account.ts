import {createClient} from '@supabase/supabase-js';
import {passwordAllowed,passwordScore} from '../lib/account-policy';

const root=document.querySelector<HTMLElement>('#account-app');
if(root){
  const find=<T extends HTMLElement>(selector:string)=>root.querySelector<T>(selector)!;
  const setup=find<HTMLElement>('#setup-state');
  const controls=find<HTMLElement>('#auth-controls');
  const signed=find<HTMLElement>('#signed-in');
  const heading=find<HTMLElement>('#form-heading');
  const intro=find<HTMLElement>('#form-intro');
  const status=find<HTMLElement>('#account-status');
  const email=find<HTMLInputElement>('#account-email');
  const password=find<HTMLInputElement>('#account-password');
  const confirm=find<HTMLInputElement>('#account-confirm');
  const confirmWrap=find<HTMLElement>('#confirm-wrap');
  const strength=find<HTMLElement>('#strength');
  const submit=find<HTMLButtonElement>('#account-submit');
  const createTab=find<HTMLButtonElement>('#mode-create');
  const signInTab=find<HTMLButtonElement>('#mode-signin');
  const recovery=find<HTMLButtonElement>('#password-recovery');
  const google=find<HTMLButtonElement>('#google-signin');
  let mode:'create'|'signin'|'reset'='create';
  let busy=false;
  let googleEnabled=false;
  const url=root.dataset.url?.trim()||'';
  const key=root.dataset.key?.trim()||'';
  const configured=Boolean(url&&key&&/^https:\/\/[a-z0-9-]+\.supabase\.co\/?$/i.test(url));
  const redirectTo=new URL('/account/',location.origin).href;
  const setStatus=(text:string,isError=false)=>{status.textContent=text;status.classList.toggle('error',isError);};
  const setBusy=(value:boolean)=>{
    busy=value;
    for(const button of [createTab,signInTab,google,recovery,submit])button.disabled=value;
  };
  const showStrength=()=>{
    const score=passwordScore(password.value);
    strength.dataset.score=String(score);
    const hint=strength.querySelector('small')!;
    hint.textContent=score>=4?'Strong password':score>=2?'More length can make this stronger.':'Use at least 15 characters. A long passphrase works well.';
  };
  const setMode=(next:'create'|'signin'|'reset')=>{
    mode=next;setStatus('');
    createTab.setAttribute('aria-pressed',String(next==='create'));
    signInTab.setAttribute('aria-pressed',String(next==='signin'));
    heading.textContent=next==='create'?'Create your space.':next==='reset'?'A fresh start.':'Welcome back.';
    intro.textContent=next==='create'?'A free account to make Floatlet yours. No subscription.':next==='reset'?'Choose a new password for your Floatlet account.':'Pick up where you left off.';
    confirmWrap.hidden=next==='signin';
    strength.hidden=next==='signin';
    password.autocomplete=next==='create'?'new-password':'current-password';
    if(next==='reset')password.autocomplete='new-password';
    find<HTMLElement>('#email-wrap').hidden=next==='reset';
    find<HTMLElement>('.mode-tabs').hidden=next==='reset';
    find<HTMLElement>('.divider').hidden=next==='reset'||!googleEnabled;
    google.hidden=next==='reset'||!googleEnabled;
    find<HTMLElement>('.legal-note').hidden=next==='reset';
    recovery.hidden=next!=='signin';
    submit.innerHTML=next==='create'?'Create free account <span>↗</span>':next==='reset'?'Save new password <span>↗</span>':'Sign in <span>↗</span>';
  };
  createTab.addEventListener('click',()=>setMode('create'));
  signInTab.addEventListener('click',()=>setMode('signin'));
  password.addEventListener('input',showStrength);
  find<HTMLButtonElement>('#password-visibility').addEventListener('click',event=>{
    const button=event.currentTarget as HTMLButtonElement;
    password.type=password.type==='password'?'text':'password';
    button.textContent=password.type==='password'?'Show':'Hide';
    button.setAttribute('aria-label',password.type==='password'?'Show password':'Hide password');
  });
  if(!configured){
    setup.hidden=false;controls.hidden=true;
    heading.textContent='Your account is on its way.';
    intro.textContent='The Floatlet account service is not connected yet.';
  }else{
    const client=createClient(url.replace(/\/$/,''),key,{auth:{flowType:'pkce',detectSessionInUrl:true,autoRefreshToken:true,persistSession:true}});
    google.hidden=true;find<HTMLElement>('.divider').hidden=true;
    void fetch(url.replace(/\/$/,'')+'/auth/v1/settings',{headers:{apikey:key}})
      .then(async response=>response.ok?response.json():null)
      .then(settings=>{googleEnabled=settings?.external?.google===true;if(mode!=='reset'){google.hidden=!googleEnabled;find<HTMLElement>('.divider').hidden=!googleEnabled;}})
      .catch(()=>{});
    const recovering=()=>mode==='reset';
    const renderUser=async()=>{
      if(recovering())return;
      const {data,error}=await client.auth.getUser();
      if(recovering())return;
      const user=!error?data.user:null;
      signed.hidden=!user;controls.hidden=Boolean(user);
      if(user){
        heading.textContent='Welcome in.';
        intro.textContent='Your Floatlet account is ready.';
        find<HTMLElement>('#signed-in-email').textContent=user.email||'Connected account';
      }
    };
    void renderUser();
    client.auth.onAuthStateChange(event=>{
      if(event==='PASSWORD_RECOVERY'){setMode('reset');signed.hidden=true;controls.hidden=false;password.focus();return;}
      setTimeout(()=>void renderUser(),0);
    });
    google.addEventListener('click',async()=>{
      if(busy||!googleEnabled)return;
      setBusy(true);setStatus('Opening Google sign-in…');
      try{
        const {error}=await client.auth.signInWithOAuth({provider:'google',options:{redirectTo}});
        if(error)setStatus(error.message,true);
      }catch{setStatus('Google sign-in could not be opened. Try again shortly.',true);}
      finally{setBusy(false);}
    });
    recovery.addEventListener('click',async()=>{
      if(busy)return;
      if(!email.validity.valid){setStatus('Enter your email address above, then request a reset link.',true);email.focus();return;}
      setBusy(true);setStatus('Requesting a reset link…');
      try{
        const {error}=await client.auth.resetPasswordForEmail(email.value.trim(),{redirectTo});
        setStatus(error?error.message:'If an account uses this address, a password reset link will arrive by email.',Boolean(error));
      }catch{setStatus('The account service could not be reached. Try again shortly.',true);}
      finally{setBusy(false);}
    });
    find<HTMLFormElement>('#account-form').addEventListener('submit',async event=>{
      event.preventDefault();if(busy)return;
      const address=email.value.trim().toLowerCase();
      if(mode!=='reset'&&!email.validity.valid){setStatus('Enter a valid email address.',true);email.focus();return;}
      if(!password.value){setStatus('Enter your password.',true);password.focus();return;}
      if(mode!=='signin'&&!passwordAllowed(password.value)){setStatus('Use at least 15 characters and avoid common or repeated passwords. A long passphrase works well.',true);password.focus();return;}
      if(mode!=='signin'&&password.value!==confirm.value){setStatus('Those passwords do not match.',true);confirm.focus();return;}
      setBusy(true);setStatus(mode==='create'?'Creating your account…':mode==='reset'?'Saving your new password…':'Signing you in…');
      try{
        if(mode==='reset'){
          const {error}=await client.auth.updateUser({password:password.value});
          if(error){setStatus(error.message,true);return;}
          password.value='';confirm.value='';setMode('signin');
          await renderUser();setStatus('Your password has been updated.');return;
        }
        const result=mode==='create'
          ?await client.auth.signUp({email:address,password:password.value,options:{emailRedirectTo:redirectTo}})
          :await client.auth.signInWithPassword({email:address,password:password.value});
        if(result.error){setStatus(result.error.message,true);return;}
        password.value='';confirm.value='';showStrength();
        if(mode==='create'&&!result.data.session){setMode('signin');setStatus('Check your email for a confirmation link, then sign in.');}
        else{setStatus('Signed in.');await renderUser();}
      }catch{setStatus('The account service could not be reached. Try again shortly.',true);}
      finally{setBusy(false);}
    });
    find<HTMLButtonElement>('#sign-out').addEventListener('click',async()=>{
      const {error}=await client.auth.signOut();
      if(error){intro.textContent='Sign-out could not be completed. Try again shortly.';return;}
      signed.hidden=true;controls.hidden=false;setMode('signin');
      heading.textContent='Welcome back.';
      intro.textContent='You have signed out of this browser.';
    });
  }
}
