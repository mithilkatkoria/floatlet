# Validation history

The following resource observations were recorded for 0.3.0, before the Floatlet rename. They are not new 0.4.0 measurements.\n\nValidated on Windows 11 ARM64 using the x64 executable under emulation, with three connected monitors and mixed DPI.

- Release build: four CTest suites passed (model, storage, integrations, calendar).
- Interactive checks covered controls, preferences, timer countdown and compact state, nearby Wi-Fi display, and the Windows Night light destination.
- Calendar recurrence and timezone checks use synthetic events. A private live subscription was not supplied, so account sync has not been validated with the user's calendar.
- A 30.63 second installed-process sample measured 0.199% total-machine CPU, 46.58 MiB private memory at the end (46.93 MiB sampled peak), and 77.95 MiB working set. Playback and pointer activity were uncontrolled. This is a short observation, not a guaranteed idle budget.
- Battery drain, GPU use and 120 Hz animation pacing were not measured. Third-party caller identity and call commands are not implemented. Network switching and live call muting were not exercised during normal user activity.

Reproduce with the build script, interactive graphics/window tests and `scripts/measure_background.ps1`. Keep personal diagnostics and screenshots out of public contributions.

