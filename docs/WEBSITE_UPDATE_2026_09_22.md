# Website and desktop update, 22 September 2026

## Delivered

The existing desktop installation was updated with the tested x64 development build, preserving settings and tray references. Its executable hash matches the build, with one installed process running. Native Search was visually reviewed; actual Chrome and Drive icons appeared above matching folder results. See SEARCH_REDESIGN_2026_09_20.md for native test counts and remaining file-search latency and multi-monitor limitations.

The website was published to https://floatlet.vercel.app/ using the existing Vercel project. Production deployment: dpl_6AQAbpvBMRLxSSDW3gk5Q3Eno28R. The public Windows release remains 0.6.3; the site clearly distinguishes development features from that download.

## Website changes

- Expanded product explanation with responsive feature cards, a Search section and useful FAQs.
- Reworked sample Search with app-first examples, keyboard navigation, result selection feedback and a native-inspired visual style.
- Music-provider samples for Spotify, Apple Music and YouTube, retaining artwork as the primary visual.
- Improved minimize/restore transitions, alert presentation, clock progress, small-screen navigation and reduced-motion handling.
- Added a Search guide and updated capability, privacy, installation and release-status content.
- Added large social previews, page and application structured data, more useful descriptions and internal links. Updated the existing llms.txt factual summary.
- Kept optional tracking disabled. No real files, media accounts or calendars are accessed by the playground.

## Verification

Astro check: zero errors, warnings or hints. Six website tests passed. Static build succeeded. The built-site audit checked all 13 pages for unique titles, one H1, metadata, canonical URLs, parseable JSON-LD, existing social images and valid internal links and anchors.

Browser review covered desktop, 390-pixel and 320-pixel layouts, with no horizontal document overflow in the narrow layouts. Search filtering and arrow/Enter selection worked. A timer alert remained active after its initial expansion and cleared only after Dismiss. Minimize exposed Restore and restored successfully. Music-provider switching preserved artwork. The live production Search dialog was verified after deployment with no browser errors.

Search implementation follows the principles in Google's official AI features guidance: https://developers.google.com/search/docs/appearance/ai-features . Discoverability improvements are not a ranking or indexing guarantee. Search Console verification, live field performance and ranking changes were not measured.
