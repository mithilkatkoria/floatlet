# Floatlet launch status

Updated 12 September 2026. Live address: https://floatlet.vercel.app

## Passed

- Static Astro build, type checking, five policy/state tests and local link validation.
- Desktop, 360 px and 390 px layout checks; sample music, tray add/remove, mute, reset and privacy dialog.
- Native 0.5.0 build and all five native suites. Installed locally with settings preserved.
- Public Vercel deployment, HTTPS, security headers and real HTTP 404 response.
- Actual release manifest with file size and checksum. Downloads are ordinary links.
- Public product pages are indexable at the owner's request. Canonical URLs, sitemap, descriptive Windows guide, software metadata and llms.txt are provided. Draft privacy and terms pages remain noindex.
- Tracking disabled. No Google Analytics or Clarity IDs are configured.

## Failed and fixed during review

- A broad demo selector added aria-pressed to its container. Restricted it to buttons.
- Chapter labels had insufficient contrast. Darkened their text.
- Astro inlined the privacy script, conflicting with CSP. Disabled automatic asset inlining so executable scripts remain external.

## Not tested

- Search rankings, real-user Core Web Vitals, search-engine indexing and AI-answer inclusion.
- A complete screen-reader audit, exhaustive touch hardware testing and real-world battery/GPU usage.
- Windows 10 or older. A native ARM64 binary is not supplied. Windows 11 ARM uses x64 emulation.
- Actual Google Calendar credentials, every PC/display combination, and third-party call controls.

## Remaining owner inputs

- Search Console and Bing Webmaster ownership verification, then submit https://floatlet.vercel.app/sitemap.xml. No account submissions are claimed.
- Approved native screenshots/recording and final operator privacy/legal details. The website labels its demo as sample content.
- Analytics IDs only if measurement is wanted. Keep tracking disabled until the privacy notice and actual vendor request behavior are reviewed.

Lighthouse reports are saved locally under audit. The first mobile run found the issues recorded above; its process also reported an Edge temporary-profile cleanup error after writing the report. Scores are lab observations, not ranking or performance guarantees.

Final reports: mobile and desktop completed on 12 September 2026. See reports/lighthouse-summary.json for category scores and the CLI cleanup limitation. GitHub Windows x64 and Website jobs passed for commit 7fc3859. The installed EXE hash matches the published 0.5.0 EXE. Live sitemap contains nine URLs; homepage metadata is index,follow. Browser console reported no errors in the final interaction check.

0.6.0 update: all five native suites passed, including 38 clock/reminder assertions covering persistent alerts, five-minute eligibility, duplicate suppression, changed starts, expired events and all-day events. Website typecheck, five tests, build and 184 local links passed. Windows StartupApproved confirmed enabled. No personal calendar subscription is connected yet; Google feed latency and account-level sync remain unverified. Main download is the standalone EXE; ZIP remains secondary.
