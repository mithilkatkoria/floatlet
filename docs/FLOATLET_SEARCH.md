# Floatlet Search development guide

Floatlet Search is being added to the existing native C++ Windows app. This branch is a development preview, not a published Search release. See BUILD_STATUS.md for verification results.

## Opening Search

The default shortcut is Alt+Space. Legacy either-side shortcuts use RegisterHotKey and report conflicts. The recorder distinguishes Left and Right Alt, Ctrl and Shift. A side-specific shortcut uses a small keyboard hook that only consumes the configured chord, suppresses repeats and passes other keys through. It does not record typing. Windows-reserved chords are excluded. If Flow Launcher already owns it, open Floatlet Search from the island context menu or notification-area menu. Choose Search preferences, then Record shortcut, hold the modifiers and press the final key. Escape cancels recording, and Save applies it. Use Right Alt + Space is also available as a preset. Right Alt is treated as AltGr on layouts that synthesize Left Ctrl. A side-specific shortcut can still conflict with another app using that exact chord; choose a distinct chord in that case.

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

Search preferences has two engines:

- **Everything** preserves the existing IPC integration and Windows Search fallback. Floatlet does not change Everything settings, start it automatically or install it. Windows Search coverage depends on Windows indexing settings.
- **Floatlet built-in** is an independent filename index. It does not require Everything or Windows Search. Choose Personal folders (Desktop, Documents, Downloads, Pictures, Music and Videos) or Local fixed drives. The initial scan starts with the first non-empty search. Cached filename metadata is stored under the current user's LocalAppData/DelightIsland directory, never in the repository. File contents and queries are not cached.

The built-in engine uses a low-priority worker, a 16 MiB metadata budget and a maximum of 100,000 items. A rebuild can temporarily retain the old and new snapshots together. These are index limits, not a limit for the entire app's RAM. The status line reports a reached limit, including after a cached restart. Use Everything for larger collections. It excludes network roots, does not follow linked/reparse subfolders, skips inaccessible locations and limits traversal depth. Cloud-only files may be listed without being available offline.

Directory-name notifications determine whether another scan is needed. On searches, updates are checked at most once per two minutes. No periodic scan runs when idle. Ctrl+R explicitly refreshes the built-in index. A cache written within the last two minutes opens without a full rebuild. An older cache is shown immediately while a scan updates it. A refresh runs in the background, then updates the visible results; it is not an instantaneous filesystem journal. Switching away cancels the scan and releases the index and notification handles. Deleted results are filtered before display. Exact filename/path token matches work in built-in mode; file typo recovery remains an Everything feature. Application typo matching remains available in either mode.

Everything's specialized index can cover huge drives faster. The built-in engine is a bounded, dependency-free option, not a claim of equal whole-drive indexing performance.

Approximate spelling uses bounded candidate retrieval followed by local edit-distance matching. It is not a promise to recover every typo across an entire filesystem.

## Resource controls

Search has a query worker and a dormant built-in index worker, coalesces pending queries, and discards outdated results. Filename matches appear before ancestor-path expansion. Background IPC waits are bounded at ten seconds and stop when a newer query supersedes them. Search dismisses and cancels queued work when Windows locks, sleeps, or turns the display off. The worker sleeps when there is no request. Rendering is driven by window events; the opening fade ends after 160 ms and is disabled with reduced motion or Windows animation settings.

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

## Built-in provider verification (2026-09-23)

Seven CTest suites passed, including 126 checks for local search and modifier matching. The disposable fixture covers Unicode filenames, parent-path search, renamed/deleted originals, corrupt cache recovery, cancellation, switching the provider off, and persistent settings. A 3,000-file fixture averaged 2.5 ms per cached query over 100 queries on this machine. A personal-folder audit reached the 100,000-item cap in 29 to 38 seconds and averaged 3.28 to 4.69 ms per warm query. A fresh restart loaded the cache in 765 ms. These figures are from an x64 build under emulation on one computer. This is not a full-drive benchmark or battery measurement.

The `--search-settings-review` mode opens isolated review preferences for keyboard and visual checks. It does not edit normal user settings.

- [Directory change notifications](https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications)
- [Low-level keyboard hooks](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)
