# Contributing to Floatlet

Issues and pull requests are welcome. Maintained by [Mithil Katkoria](https://github.com/mithilkatkoria).

Fork the repository, create a focused branch, and explain the problem and resulting behavior in your pull request. Include the relevant tests and actual measurements when changing rendering, timers, monitoring or memory usage. Never include personal files, calendar feeds, screenshots of private applications, tokens or credentials.

Use C++20 and native Windows APIs. Keep idle work event-driven where possible. Add bounds to all external payloads. A file tray reference must never delete or move the original. Preserve per-monitor DPI and passive focus behavior. Do not add telemetry or a web runtime without first discussing the design in an issue.

Build with `./scripts/build.ps1 -Architecture x64`. Windows 11, Visual Studio 2022 C++ tools, CMake and Windows SDK 10.0.26100 are required. Run the relevant tests and describe hardware-dependent checks that were not possible. Calendar tests use synthetic data.

The main branch requires a passing Windows build and approval from the designated code owner, `@mithilkatkoria`, for outside contributions. Only the repository owner administers releases and merging permissions. Contributors can freely fork and modify the MIT-licensed project.
