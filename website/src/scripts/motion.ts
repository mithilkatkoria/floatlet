const reduced=matchMedia('(prefers-reduced-motion: reduce)');
const targets=document.querySelectorAll<HTMLElement>('.feature-row,.search-illustration,.open-visual,.home-faq,.download-banner');
if(!reduced.matches&&'IntersectionObserver' in window){
 const observer=new IntersectionObserver(entries=>{for(const entry of entries){if(!entry.isIntersecting)continue;observer.unobserve(entry.target);if(entry.boundingClientRect.top>120)(entry.target as HTMLElement).animate([{transform:'translateY(17px)',opacity:.72},{transform:'translateY(0)',opacity:1}],{duration:650,easing:'cubic-bezier(.22,1,.36,1)'})}},{threshold:.12});
 targets.forEach(target=>observer.observe(target));
 reduced.addEventListener('change',()=>{if(reduced.matches){observer.disconnect();targets.forEach(target=>target.getAnimations().forEach(animation=>animation.cancel()))}});
}