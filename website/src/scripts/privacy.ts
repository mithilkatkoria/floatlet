import { readConsent, denied, cleanEvent, type Consent } from '../lib/policy';
import { release } from '../data/release';

declare global {
  interface Window { dataLayer?: unknown[]; gtag?: (...args: unknown[]) => void }
}
const key = 'floatlet-privacy';
let choice: Consent | null = null;
try { choice = readConsent(localStorage.getItem(key)); } catch {}
let active = choice || denied();
let gaLoaded = false;
const ga = import.meta.env.PUBLIC_GA4_ID || '';
const enabled = import.meta.env.PUBLIC_TRACKING_ENABLED === 'true'
  && import.meta.env.MODE === 'production' && /^G-[A-Z0-9]+$/.test(ga)
  && location.origin === (import.meta.env.SITE_ORIGIN || 'https://floatlet.vercel.app')
  && !['/account/', '/admin/'].some(path => location.pathname.startsWith(path));
const dialog = document.querySelector<HTMLDialogElement>('#privacy-dialog')!;
const analytics = document.querySelector<HTMLInputElement>('#analytics-choice')!;
const banner = document.querySelector<HTMLElement>('#privacy-banner')!;
const status = document.querySelector<HTMLElement>('#privacy-status')!;

function load() {
  if (!enabled || !active.analytics || gaLoaded) return;
  gaLoaded = true;
  window.dataLayer = window.dataLayer || [];
  window.gtag = function () { window.dataLayer!.push(arguments); };
  // Basic consent mode: no Google script or requests exist before opt-in.
  window.gtag('consent', 'default', {
    analytics_storage: 'granted', ad_storage: 'denied',
    ad_user_data: 'denied', ad_personalization: 'denied',
  });
  window.gtag('js', new Date());
  // Never send URL queries, fragments, account URLs or user-entered content.
  window.gtag('config', ga, {
    send_page_view: false, allow_google_signals: false,
    allow_ad_personalization_signals: false,
    page_location: location.origin + location.pathname,
    page_referrer: safeReferrer(document.referrer),
    cookie_domain: location.hostname, cookie_expires: 180 * 86400,
    cookie_update: false,
  });
  window.gtag('event', 'page_view', {
    page_title: document.title, page_location: location.origin + location.pathname,
    page_referrer: safeReferrer(document.referrer),
  });
  const script = document.createElement('script');
  script.src = 'https://www.googletagmanager.com/gtag/js?id=' + ga;
  script.async = true;
  document.head.append(script);
}
function safeReferrer(value: string) {
  try { const url = new URL(value); return /^https?:$/.test(url.protocol) ? url.origin + '/' : ''; }
  catch { return ''; }
}
function openSettings() {
  analytics.checked = active.analytics;
  analytics.disabled = !enabled;
  status.textContent = enabled
    ? 'Allow Google Analytics to measure page visits, demo interactions and download clicks. Downloads work either way. No advertising or session recording.'
    : 'Optional analytics is not enabled on this version of the site. Downloads and the demo work without tracking.';
  dialog.showModal();
}
function clearAnalyticsCookies() {
  for (const cookie of document.cookie.split(';')) {
    const name = cookie.split('=')[0].trim();
    if (name === '_ga' || name.startsWith('_ga_')) {
      for (const domain of ['', location.hostname, '.' + location.hostname]) {
        document.cookie = name + '=; Max-Age=0; path=/;' + (domain ? ' domain=' + domain + ';' : '');
      }
    }
  }
}
function save(allow: boolean) {
  const withdraw = gaLoaded && !allow;
  active = { ...denied(), analytics: enabled && allow };
  try { localStorage.setItem(key, JSON.stringify(active)); } catch {}
  banner.hidden = true;
  dialog.close();
  if (withdraw) {
    // Stop collection before clearing cookies. Reload removes Google's listeners.
    (window as unknown as Record<string, unknown>)['ga-disable-' + ga] = true;
    clearAnalyticsCookies();
    location.reload();
    return;
  }
  if (!allow) clearAnalyticsCookies();
  load();
}
document.querySelector('#privacy-open')!.addEventListener('click', openSettings);
document.querySelector('#privacy-settings')!.addEventListener('click', openSettings);
document.querySelector('#privacy-save')!.addEventListener('click', e => { e.preventDefault(); save(analytics.checked); });
document.querySelector('#privacy-reject')!.addEventListener('click', e => { e.preventDefault(); save(false); });
document.querySelector('#privacy-accept')!.addEventListener('click', e => { e.preventDefault(); save(true); });
document.querySelector('#privacy-banner-accept')!.addEventListener('click', () => save(true));
document.querySelector('#privacy-banner-reject')!.addEventListener('click', () => save(false));

function event(name: string, params: Record<string, unknown>) {
  const safe = cleanEvent(name, params);
  if (safe && enabled && active.analytics && gaLoaded) window.gtag?.('event', safe.name, safe.params);
}
document.addEventListener('floatlet-event', e => {
  const v = (e as CustomEvent).detail;
  if (v && typeof v.name === 'string' && v.params && typeof v.params === 'object') event(v.name, v.params);
});
document.addEventListener('click', e => {
  const target = e.target instanceof Element ? e.target.closest<HTMLElement>('[data-event]') : null;
  if (target) event(target.dataset.event!, {
    placement: target.dataset.placement || 'body', architecture: 'x64',
    version: release?.version || '', package_type: target.dataset.packageType || 'exe',
  });
});
if (!['/account/', '/admin/'].some(path => location.pathname.startsWith(path))) {
  load();
  banner.hidden = !enabled || !!choice;
}
