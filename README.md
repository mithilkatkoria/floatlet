# Floatlet

[![Windows build](https://github.com/mithilkatkoria/floatlet/actions/workflows/build.yml/badge.svg)](https://github.com/mithilkatkoria/floatlet/actions/workflows/build.yml)

A compact Dynamic Island companion for Windows 10 and 11. Music, a temporary file tray, quick controls, timers and a calendar above your workspace. Built in C++20 with native Windows composition, without Electron or a bundled browser.

Created by [Mithil Katkoria](https://github.com/mithilkatkoria). Independent open-source software, unaffiliated with Apple, Microsoft or integrated applications.

**[Website](https://floatlet.vercel.app) | [Download](https://github.com/mithilkatkoria/floatlet/releases)** | [Contribute](CONTRIBUTING.md) | [MIT license](LICENSE)

## Features

| Feature | Behavior |
| --- | --- |
| Music | Windows media-session artwork, artist and title; previous, play/pause and next on hover. Optional icon-only resting view. |
| File tray | Drop files, drag references back out, remove references and browse recent saved screenshots. Originals remain in place. |
| Controls | Volume slider and mute, supported internal-display brightness, nearby Wi-Fi and system status. |
| Clocks | Timer presets, stopwatch with compact live count, alarms and expanding completion alerts with a dismissible chime. |
| Calendar | Month navigation and double-click day agenda from a read-only Google Calendar subscription. |
| Microphone | Event-driven activity detection for selected communication apps and browsers, with endpoint mute. No audio recording. |
| Multiple monitors | Pointer following with per-monitor DPI handling. Dragging and menus hold position. |
| Preferences | Compact music, pointer following, touch targets, reduced motion and microphone options. |

Runs outside the taskbar and Alt+Tab. Hover to preview, click to expand, and leave an unpinned panel to return to compact music. Pin preserves the current panel. Use the system tray icon to quit.

## Install

Extract the release ZIP. Run `Floatlet.exe` for portable use, or install for your Windows user:

```powershell
.\install.ps1 -AcceptDefaults -Launch
```

New installations start with Windows by default; use `-NoStartup` to opt out. Upgrades preserve startup preferences and existing data. Releases are unsigned previews. The x64 executable runs on Intel/AMD Windows and through emulation on Windows on ARM. A native ARM64 release is not supplied yet.

The gear opens preferences. Ctrl+Shift+F12 opens with keyboard focus, Escape collapses, and Ctrl+Alt+H hides or shows. Right-click opens additional actions. Exit before running the installed `uninstall.ps1`; uninstall retains settings.

## Calendar subscriptions

In Google Calendar on the web, open **Settings > Settings for my calendars > your calendar > Integrate calendar**. Copy its **Secret address in iCal format** into Floatlet's **Preferences > Google Calendar** dialog. Treat that address as a password; never include it in issues or screenshots.

Use Add calendar for each link, then Save. Up to eight Google Calendar subscriptions share one chronological agenda and upcoming reminders. Select a saved calendar and choose Remove selected, then Save, to disconnect it. Existing single-calendar connections are preserved.

Windows DPAPI encrypts links locally for your user. One background worker checks connected calendars about once per minute and caches parsed events in memory. Duplicate links and matching event occurrences are filtered out. A failed feed does not prevent other calendars from syncing, and stale offline events do not trigger new reminders. Double-click a day to open its agenda; Refresh requests an update. No events are uploaded, created or edited.

Common daily, weekly, monthly and yearly recurrence patterns, exceptions and overrides are supported. Unsupported patterns are reported. This is not a complete RFC 5545 client. Only Google Calendar subscription URLs are accepted in this version.

## Integration limits

- Nearby Wi-Fi connections use saved profiles. New credentials use Windows. Windows may require location permission and enabled radios.
- Night light opens its exact Windows settings page. Bluetooth pairing, projection and energy saver use Windows interfaces, not embedded toggles.
- Brightness requires Windows WMI support, generally for the internal display. External monitor DDC/CI is not implemented.
- Microphone activity does not prove a call. Mute affects the microphone across apps. Caller identity, photos, answering and hang-up are not exposed.
- AirPods audio connection can show a notice. Battery and charging values are unavailable and are not fabricated.
- The tray holds references, not copies or a clipboard archive. Screenshot browsing finds saved files; clipboard-only captures are not automatically retained.
- Timers require the app to remain running and do not schedule a wake from sleep. Full custom-control accessibility and verified 120 Hz frame pacing remain unfinished.

## Performance

Direct2D, DirectWrite and Windows composition render the interface. Drawing surfaces and text formats are reused. Slider feedback updates in the UI while brightness writes are coalesced on a worker. Media and microphone services use notifications. Statistics are sampled while controls are visible; countdown ticks run only when needed. Monitor following still requires lightweight pointer polling.

Zero CPU, RAM or battery use is impossible. `scripts/measure_background.ps1` measures process CPU and memory under the current conditions. Working set and private memory differ. GPU activity, frame pacing and battery drain require separate measurements; a short CPU sample does not establish battery impact.

## Build and test

Requires Windows 10 or 11, Visual Studio 2022 C++ desktop Build Tools, its CMake component and Windows SDK 10.0.26100. The build script downloads no dependencies.

```powershell
.\scripts\build.ps1 -Architecture x64
.\out\x64\Release\graphics_tests.exe
.\out\x64\Release\window_tests.exe
.\scripts\package.ps1 -Architecture x64
```

The build runs model, storage, integration, calendar and clock tests. Graphics and real-window tests require an interactive desktop. ARM64 builds require the corresponding MSVC tools; pass `-Architecture arm64` when installed. CI builds and tests x64 on Windows.

`src/app` coordinates the window; `src/ui` owns layout and drawing; `src/services` integrates Windows and calendar data; `src/platform` handles windows and file transfer; `src/storage` persists settings. Compatibility identifiers and the settings directory retain the original internal name `DelightIsland`.

## Community

Bug reports and focused pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md) and the [changelog](CHANGELOG.md). The owner maintains releases and reviews contributions. Licensed under [MIT](LICENSE).
