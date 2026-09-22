# Floatlet refinement, 19 September 2026

Installed the local x64 development build with startup enabled. A rollback executable is retained at `out/Floatlet-before-refinement.exe`. This is not a new public installer release and does not establish native ARM64 compatibility.

- Repeated lock/suspend events no longer replace the saved pre-suspend state with Unavailable.
- Tray Show explicitly restores a visible collapsed island.
- Hover exposes a minimize button. The island shrinks and moves to a right-edge restore tab using finite spring animation. Click the tab to restore.
- Music visualizer polling stops while minimized. The later September 20 refinement keeps pointer following active while minimized when enabled. Timers and alarms continue running.
- Alt+Space Search uses the existing conflict-aware registration and can also be opened from the tray.

Verification: six CTest suites passed, 12,100 native geometry assertions passed on two connected displays, and all 918 Search runtime checks passed. Search handles stayed at 468 and GDI objects at 53. Installed HWND was visible, a tool window, and absent from the taskbar. The reported disappearance has a plausible recovery fix; a full physical lock/sleep and hover-animation interaction test is still required. The Alt+Space keyboard gesture itself was not manually retested in this run.

Website adds a sample Search dialog and minimize/restore preview. Astro check returned no errors or warnings, six web tests passed, and static build succeeded. Sample Search never reads visitor files. Public download metadata remains on the existing released build.

## Follow-up regression repair

The initial minimize implementation did not update HWND position during animation ticks. Corrected it to animate the visible centre, keeping the input mask aligned with the canvas envelope. Added real intermediate-position, settled-edge hit-region and restore-position assertions. All 12,112 native window checks passed on two monitors. An isolated preview was minimized with the hover button and restored by clicking the edge tab. Tray click also restores a minimized island; pending hover timers are cancelled during minimize/restore.

Search now publishes application icons before indexed-file/path queries and publishes file icons before ancestor-path expansion. Icon-only updates repaint existing rows without rebuilding the list. Commands can appear before a cold application catalogue finishes. Distinct command symbols replace identical placeholder boxes.

All six CTest suites passed. The 918-check runtime run passed with stable handles (424) and GDI objects (53), median query-plus-show 10.92 ms, p95 18.23 ms and worst 3013.30 ms. The slow file-query tail is still a limitation, not an instant-search guarantee. The installed Alt+Space gesture was exercised from Explorer and the Search window was visible. Final fixes were installed locally, with existing settings preserved. No new public release or website deployment is claimed.

## Application priority and visual rendering

Matching applications now receive priority above file/folder results. Added a regression for Google Chrome versus an exact Google folder and checked the real query in the preview. Chrome and Drive appeared above folders with actual icons. Shell icons now request the large variant instead of enlarging a small icon. Failed icon lookups are not cached permanently.

Search uses a charcoal/blue palette, Segoe UI Variable, softer row corners and no large white native scrollbar (wheel and keyboard navigation remain). The island uses a composition rounded geometric clip inside its HWND input region to anti-alias the shell. Missing media covers use a vector music symbol; application logos are no longer fetched as album artwork. Media selection prefers an actively playing session when the current one is inactive. Blank metadata updates retain the existing same-source title when available. Apple Music web-specific verification remains pending; browser metadata availability determines what can be shown.

Six CTest suites and composition graphics test passed. Search runtime passed 918 assertions; median 42.28 ms, p95 79.28 ms, worst 2039.87 ms on this run. Handles rose from 490 to 500 then plateaued; GDI objects remained 54. These observations do not establish zero resource use or consistently instant file queries. Visual reference consulted: https://www.flowlauncher.com/ . This is a local installed development update, not a new public release.
