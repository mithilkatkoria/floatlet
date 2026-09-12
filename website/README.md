# Floatlet website

Static Astro website for the native Windows companion. The interactive island uses original sample content. It does not read files, control audio or connect to native services.

## Development

Requires Node.js 22 or newer. Run `npm ci`, `npm run dev`, and open the printed localhost URL. Before deployment run `npm run check`, `npm test`, and `npm run build`.

Vercel configuration is included. Run the Vercel CLI from this directory. Set `SITE_ORIGIN` to the verified HTTPS domain without a trailing slash. Public product pages use canonical URLs and indexable metadata. Set SITE_MODE=preview for a noindex review build. Optional tracking is disabled. No analytics IDs or private calendar links belong in this repository.

## Content and releases

`src/data/release.json` describes the actual published Windows download, including its SHA-256 hash. Update it only after a release exists. `src/data/pages.json` contains support, capabilities and policy pages. Feature claims are deliberately narrower than planned features. The app installed locally can contain newer changes than the public release.

The website inherits the repository's MIT license. The pill mark, sample artwork and sample track titles are original. Fonts use the operating system stack. There are no third-party photographs, album covers or recordings in the site.

## Verification and launch status

On 12 September 2026, Astro type checking and five policy/demo tests passed. Browser checks covered the music, sample tray, controls, mute and privacy dialog at desktop and 390 px mobile width. The controls panel fit without horizontal overflow. Reset restores sample playback labels and sliders.

The public website describes an unsigned software preview. Approved native screenshots, a recording and final operator policy details remain outstanding. Draft policy pages remain noindex. Tracking stays disabled until its IDs and actual network behavior are verified. `scripts/gate.mjs` validates the release and prevents silently enabling tracking before that work. No battery savings or zero-resource claims are made.
