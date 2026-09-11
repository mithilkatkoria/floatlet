# Changelog

## 0.3.0 - Glide Island

- New Glide Island name, executable, native icon and per-user installer migration.
- Music opens by default after leaving an unpinned panel. Pinned panels retain their place.
- Drawing surfaces and text formats are reused. Slider feedback updates at up to 60 Hz during a drag; brightness writes are coalesced on a worker.
- Countdown timer with presets, minute adjustment, pause/resume, compact progress ring and completion alert.
- In-island preferences with appearance, pointer following, touch, motion and microphone options.
- Nearby Wi-Fi scanning with signal strength, connection status and saved-profile connection requests.
- Read-only Google Calendar subscription with encrypted local link storage and double-click day agenda.
- Event-driven microphone session detection for selected calling apps and browsers, with endpoint mute control.
- Existing music artwork, hover transport, file tray, screenshot references, calendar and multi-monitor behavior preserved.

Known limits: Night light, Bluetooth pairing, projection and energy saver use Windows interfaces. Caller identity, photos, answering and ending calls are not exposed by this integration. AirPods battery and charging are not reported. The calendar supports common iCalendar recurrence patterns, not every RFC 5545 feature. The timer runs while the app is running, with no scheduled wake from sleep. Full custom-control accessibility and 120 Hz frame pacing remain unfinished.
