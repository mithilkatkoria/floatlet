# Apple Music web icon

The app draws a crisp red vector music icon for native Apple Music sessions and browser sessions with an Apple Music top-level window title belonging to the same browser executable. This avoids presenting Chrome artwork as the player brand in that case. Metadata and playback still use the existing Windows media session.

Identification is a best-effort window-title heuristic, checked on media metadata updates. It does not inspect browser history or page contents and adds no polling timer. A hidden background tab may not identify itself. Multiple media sites open in separate windows of the same browser can be ambiguous. This does not provide definitive per-tab attribution or cover-art recovery.

Six CTest suites passed, including native/browser name matching checks. The composition smoke test passed. No Apple Music window was exposed during desktop verification, so live Apple Music web identification remains unverified. Installed locally on 20 September 2026.

## Album-cover correction

Album artwork now takes priority over provider icons. For browser/Apple Music sessions without cached artwork, a worker performs an HTTPS catalogue search using the current title and artist. It accepts only normalized exact title and artist matches, downloads from Apple artwork hosts only, and bounds JSON at 256 KiB and image data at 4 MiB. Redirects are disabled and requests have timeouts. Images stay in the existing in-memory 128-pixel artwork buffer; no listening history or artwork files are written. The catalogue title/artist are sent to Apple, so this feature is not entirely offline. Network or match failures retain the supplied media thumbnail or vector fallback.

Verified the screenshot example Miss Independent by Ne-Yo through the native HTTP implementation (6225 image bytes returned). The live preview displayed album art for Hey Daddy by USHER during playback. All six CTest suites passed. Catalogue matches cannot guarantee the exact album edition when the same artist releases multiple recordings with the same title. No public release is claimed.
