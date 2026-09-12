# Changelog

## 0.6.1 - App icons and multiple calendars

- Use the Windows app logo when a media session supplies no artwork, with a music-note fallback instead of the Floatlet capsule logo.
- Connect up to eight Google Calendar subscription links with individual add/remove controls and encrypted local storage.
- Merge calendar events chronologically, filter duplicate occurrences and continue syncing healthy feeds when another feed is offline.
- Preserve existing single-calendar subscriptions and five-minute reminders.

## 0.6.0 - Persistent alerts and calendar reminders

- Icon-only music is slightly wider (84 DIP), with the same small waveform.
- Alarm and timer alerts ring until Dismiss is pressed inside Clock. Opening the pill, leaving it or pressing Escape does not dismiss them. Alert icons shake briefly every few seconds and respect reduced motion.
- Camera and calendar icons distinguish notifications. Alerts expand then return to a compact active state.
- Google Calendar subscriptions refresh about once per minute. Upcoming timed events show a reminder within five minutes of their start, once per occurrence per app session. Google feed publication delays still apply; offline data does not trigger new reminders.
- Guided calendar setup and Ctrl+Shift+F12 open shortcut to avoid the previous Flow Launcher conflict.
- Main website download is now the standalone Floatlet.exe. The ZIP with installation scripts remains available.
- Explicit installer option -EnableStartup enables Windows startup for upgrades while preserving app settings.

## 0.5.0 - Clocks and quiet motion

- Running stopwatch stays visible in the compact island. Timer completion and alarms expand into a notification and play a dismissible chime.
- Compact music uses a softly smoothed audio-level waveform with a 12 DIP maximum height and muted lavender color. It does not record audio or grow the capsule.
- Larger hover playback targets, screenshot-copy notifications and a subtle settings credit.
- Published the Floatlet website with downloads, guides and an interactive sample island.
- Windows 11 x64 release for Intel and AMD PCs, also usable through Windows 11 ARM emulation. This is not restricted to Surface hardware. Hardware-specific controls remain conditional.

## 0.4.0 - Floatlet

- Renamed the app, executable, shortcuts and public project to Floatlet.
- Kept existing settings, calendar credentials, tray references and startup preferences during upgrades.
- Replaced the idle label with a minimal black pill and handled shortcut conflicts without a startup popup.
- Retained music controls, tray, timers, preferences and read-only calendar integration from 0.3.0.

## 0.3.0

Added quick timers, nearby Wi-Fi, in-island preferences, calendar subscriptions, microphone activity controls and rendering reuse.

Limits: Night light, Bluetooth pairing, projection and energy saver use Windows interfaces. Caller identity, photos, answering and ending calls are not implemented. AirPods battery and charging are not reported. Calendar recurrence support is partial. Timers require a running app. Full custom-control accessibility and verified 120 Hz frame pacing remain unfinished.
