# Security and privacy

Please report vulnerabilities using GitHub's private vulnerability reporting feature for this repository. Do not post calendar subscription URLs, account details, private screenshots or exploit data in public issues.

Glide Island does not contain analytics, advertising or a backend. The optional Google Calendar reader makes HTTPS requests directly to calendar.google.com only after a user supplies a subscription address. Redirects and cookies are disabled. The address is protected with Windows DPAPI for the current user. Calendar events remain in memory and are not included in diagnostic files. A subscription address grants access to that calendar: keep it private and reset it in Google Calendar if exposed.

Nearby Wi-Fi scans are requested by opening or refreshing the Wi-Fi panel. Windows location permissions apply. Passwords are not requested or read by Glide Island. New network setup uses Windows.

Microphone integration observes native session metadata and endpoint mute state. It does not capture audio samples, transcribe calls, read messages or access caller photos. Muting affects the physical microphone endpoint across applications.

Tray entries are path references. Outgoing drags advertise COPY only. Removing a reference keeps the original file. Screenshot history reads the Windows Screenshots folder on demand; it does not record the desktop or watch the clipboard.

Release binaries are currently unsigned. No installer changes Windows security or execution policy. Download from this repository's Releases or build the source yourself.
