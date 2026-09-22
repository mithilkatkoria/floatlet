# Floatlet Search development guide

Floatlet Search is being added to the existing native C++ Windows app. This branch is a development preview, not a published Search release. See BUILD_STATUS.md for verification results.

## Opening Search

The default shortcut is Alt+Space. Floatlet uses RegisterHotKey and does not take a shortcut away from another launcher. If Flow Launcher already owns it, open Floatlet Search from the island context menu or notification-area menu. Choose Search preferences to assign another modified key or disable Search.

Search preferences are also linked from the island Preferences panel. Applications, indexed files and folders, open windows, and approximate spelling can be enabled separately. Queries and search history are not saved to disk or sent to a server. The connection section checks Everything on demand and offers Start Everything only when an installed executable is found and no compatible IPC window is present. It never starts or installs Everything automatically. Portable and custom instances may need to be started manually.

## Find and act

Type an app name, file name, folder name, or words from a containing path. Examples include `certificate`, `maths pdf`, and `Certificates`. Files whose containing folder matches are eligible alongside directly matching files and folders. Direct name matches rank above ancestor-path-only matches.

- Up/Down and Page Up/Page Down select results.
- Enter opens the selected result or switches to its existing window.
- Tab opens the result action menu.
- Shift+Enter adds a file or folder reference to Floatlet and closes Search.
- Ctrl+Enter adds a reference while keeping Search open to collect more items.
- Escape closes Search and attempts to restore the previous foreground window.
- The action menu offers Reveal in File Explorer, Copy file, and Copy path.

Adding to Floatlet uses the existing tray and its storage limits. It keeps a reference, does not move the original, and does not create another permanent copy. Existing tray drag-out behavior is retained. Unavailable originals produce an error when acted on.

## File providers

Everything is the primary provider. Floatlet communicates through the documented Unicode WM_COPYDATA protocol with fixed-width fields. There is no Everything DLL dependency and no duplicated filesystem index. Floatlet does not change Everything settings, restart it, or automatically install it.

If the Everything IPC window is absent, the app attempts a bounded query against the existing Windows Search index. Its coverage depends on Windows indexing settings. Neither provider searches unindexed locations by recursively crawling drives. Cloud-only and offline items may be indexed but still unavailable to open.

Approximate spelling uses bounded candidate retrieval followed by local edit-distance matching. It is not a promise to recover every typo across an entire filesystem.

## Resource controls

Search has one worker, coalesces pending queries, and discards outdated results. Filename matches appear before ancestor-path expansion. Background IPC waits are bounded at ten seconds and stop when a newer query supersedes them. Search dismisses and cancels queued work when Windows locks, sleeps, or turns the display off. The worker sleeps when there is no request. Rendering is driven by window events; the opening fade ends after 160 ms and is disabled with reduced motion or Windows animation settings.

The application catalogue is capped at 4,096 entries and refreshed on a later query after five minutes, without an idle polling timer. Visible result lists are capped at 80. File queries request at most 512 records at a time. The in-memory repeated-query cache has at most eight entries, a one-second lifetime, and a 128 KiB limit per entry. The icon cache is capped at 96 entries. File icons use file-type metadata rather than reading file contents or downloading thumbnails.

These are implementation bounds, not measured guarantees about RAM, CPU, battery use, or frame pacing. Everything's own index process must be measured separately from Floatlet. The 2026-09-16 launcher-only runtime test passed 918 checks, including 100 tray cycles and 100 rapid open/close cycles with unchanged handle and GDI counts. It measured 21.98 MiB private bytes and 46.95 MiB working set. Query-plus-show latency was median 33.27 ms, p95 44.05 ms, and worst 1937.58 ms. All providers were enabled, but the measured queries use controlled file fixtures. This runs x64 under ARM64 emulation and does not measure the full island or battery drain. Earlier runs failed under load; reply handling now permits delayed Everything responses and publishes filename matches before path expansion. The outlier means these figures do not establish consistently low tail latency.

## Build and verification

A separate 30-second full-preview sample with Search closed measured 0.04% CPU, 72.22 MiB private memory and 118.56 MiB working set. Everything's GUI used a separate 345.19 MiB private memory and its service 11.34 MiB. These short x64-emulation observations do not measure battery use or establish production idle guarantees.

Run `scripts/build.ps1 -Architecture x64` or `scripts/build.ps1 -Architecture arm64` with the matching Visual Studio C++ tools installed. The build runs the deterministic CTest suites. The separate `search_ipc_tests.exe` and `search_runtime_tests.exe` require an interactive Windows session with Everything already running. They create uniquely named disposable fixtures, validate references and original files, and print counts and timings without printing private indexed results.

ARM64 source compatibility is intended, but the current local compiler installation does not contain the ARM64 toolchain. On 2026-09-16 the compiler installation required elevation and its administrator prompt was cancelled. A successful x64 run under emulation is not native ARM64 verification. Search is installed locally as a development build. It has not been publicly released. See SEARCH_REDESIGN_2026_09_20.md for the latest visual and runtime verification.

## Official references

- [Everything IPC](https://www.voidtools.com/support/everything/sdk/ipc/)
- [Everything IPC example](https://www.voidtools.com/support/everything/sdk/ipc_c_example/)
- [Everything indexing](https://www.voidtools.com/support/everything/indexes/)
- [Windows hotkeys](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)
- [Windows Shell activation](https://learn.microsoft.com/en-us/windows/win32/shell/launch)
- [Windows Search SQL](https://learn.microsoft.com/en-us/windows/win32/search/-search-sql-windowssearch-entry)
