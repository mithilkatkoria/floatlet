# Search redesign, 20 September 2026

## Design references

Reviewed the official [Flow Launcher](https://www.flowlauncher.com/) product screenshot and [Apple Spotlight guide](https://support.apple.com/en-sg/guide/mac-help/mchlp1008/mac) in the browser. Flow's visible example separates the query, icon-led results and selection clearly. Apple's example gives the search field visual priority and uses soft rounded surfaces. These informed the hierarchy, rather than introducing either product's additional features.

## Implementation

- Larger Segoe UI Variable query field, quieter result label and secondary paths.
- Native DirectWrite result titles and action labels, rounded selection and button surfaces, restrained charcoal and blue-grey colours.
- Whole result rows where space permits, with native keyboard navigation and accessible list strings retained.
- Existing providers, ranking, shortcuts, result actions and tray behaviour remain intact.
- Search follows the pointer after a stable monitor change, recalculating size from the destination display's DPI and work area. Following pauses during mouse capture, left-button use and the action menu. Display changes reconfigure the follow timer.
- The minimized island now participates in the existing pointer-follow setting and reuses its existing monitor-follow timer.
- Search's 120 ms coalesced monitor check only runs while visible on a multiple-monitor desktop. Text formats and rendering resources are reused; format caches reset when layout changes. No web engine or continuous rendering loop was added.

## Verification and remaining work

The x64 release build and all six CTest suites passed after the final changes. Added 96 placement assertions spanning 100%, 125%, 150%, 175%, 200% and 300% scaling, negative monitor coordinates, portrait displays and small work areas. These establish geometry bounds, not visual quality.

Desktop preview launch was not executed: automatic approval review could not refresh a revoked access token. Consequently the new UI has not received native screenshot review, the interactive provider and mixed-display tests have not been rerun, and this redesign has not been installed or published. Previous runtime measurements do not validate this new renderer. Resume with native visual inspection, interactive regression tests and the existing settings-preserving installer after sign-in is restored.


## Verification completed, 22 September 2026

Access was restored. Native Search was visually inspected with an empty query and Google results. Google Chrome and Drive appeared above folders with actual icons. A missing empty-field cue was repaired and visually rechecked. The x64 build again passed all six CTest suites.

Search runtime passed 918 assertions, including 100 tray cycles and 100 rapid open/close cycles. Handles were 581 before and 586 after, GDI objects 56 before and 57 after. Fixture query-plus-show timings were median 58.10 ms, p95 2553.24 ms and worst 3505.84 ms. The slow tail remains an unresolved performance limitation, not a smoothness guarantee. The test process used 51,724,288 private bytes and 85,135,360 working-set bytes.

All 6,106 real-window assertions and the composition/DirectWrite graphics check passed. Only one monitor was connected in this run, so live cross-monitor following remains unverified; the deterministic placement checks cover multiple scales and monitor arrangements.

Installed the verified executable into the existing per-user installation, preserving settings and tray references. Its SHA-256 matches the build and exactly one installed process was running after launch. This is a local development update, not a new public release.
