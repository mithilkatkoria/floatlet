const reduced=matchMedia('(prefers-reduced-motion: reduce)');
const targets=document.querySelectorAll<HTMLElement>('.feature-atlas,.search-story,.intro,.story-chapters article,.honest,.evidence,.home-faq,.get-it .wrap');
if(!reduced.matches&&'IntersectionObserver' in window){
  const observer=new IntersectionObserver(entries=>{for(const entry of entries){if(entry.isIntersecting){entry.target.classList.add('arrived');observer.unobserve(entry.target);}}},{threshold:.08});
  for(const target of targets){if(target.getBoundingClientRect().top>innerHeight){target.classList.add('reveal');observer.observe(target);}}
  reduced.addEventListener('change',()=>{if(reduced.matches){observer.disconnect();targets.forEach(t=>t.classList.add('arrived'));}});
}
