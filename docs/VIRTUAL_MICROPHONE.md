# Virtual Microphone Design

## Goal

KeyboardManager will optionally expose a Windows recording endpoint that sends
the user's physical microphone and triggered sound effects to Steam and games.
The normal keyboard manager and speaker-only soundboard must remain functional
when the driver is absent, disabled, or unhealthy.

```text
Physical microphone ----\
                         +--> user-mode mixer --> virtual render endpoint
Triggered sound effects-/                              |
                                                       v
                                            virtual capture endpoint
                                                       |
                                                       v
                                              Steam or game voice
```

Games select `KeyboardManager Virtual Microphone` as their recording device.
KeyboardManager selects the physical microphone and optional monitoring output.
The application must remain running while voice mixing is enabled; otherwise
the virtual microphone emits silence.

## Driver boundary

The prototype is based on Microsoft's MIT-licensed Simple Audio Sample pinned
to commit `197ba2156a60e2b76fcd4820bae594223e91a1e9`. It uses documented
PortCls/WaveRT interfaces and exposes a paired virtual cable:

- `KeyboardManager Mix Input`: render endpoint used only by KeyboardManager.
- `KeyboardManager Virtual Microphone`: capture endpoint selected by games.

The driver transfers frames from render to capture through a bounded nonpaged
ring buffer. It inserts silence on underrun and drops the oldest complete
frames on overflow so latency cannot grow without bound. Device stop, removal,
producer restart, sleep, and upgrade reset the buffer. The initial canonical
format is 48 kHz stereo 32-bit float with a conservative 10 ms period and a
30-50 ms target queue.

No process inspection, game hooks, code injection, physical-memory access, or
generic privileged IOCTL interface belongs in this driver. A Microsoft
signature does not guarantee acceptance by every anti-cheat vendor.

## User-mode mixer

The mixer owns all policy and conversion:

- Captures the selected physical microphone through event-driven WASAPI.
- Converts microphone input to the canonical mix format.
- Decodes triggered sounds and allows voice and effects simultaneously.
- Applies independent microphone and effects gain followed by a limiter.
- Sends the final mix to `KeyboardManager Mix Input`.
- Optionally plays effects, but not the user's microphone, to headphones.
- Stops microphone capture immediately when voice mixing is disabled.

The driver does not receive file paths, commands, configuration, or network
data. It only transports bounded PCM frames supplied through the audio render
endpoint.

## Privacy and failure behavior

- Voice mixing is opt-in and shows an unambiguous active status.
- No microphone samples are persisted or transmitted over a network.
- Logs contain state transitions and counters, never audio contents.
- If the physical microphone disconnects, effects may continue and the UI
  reports that voice input is unavailable.
- If the virtual endpoint fails, speaker playback and non-audio actions remain
  available.
- Installation failure rolls back the optional driver without breaking the
  base application.

## Installation flow

The signed release installer will:

1. Install the base application and license notices.
2. Optionally stage and install the Microsoft-signed driver package.
3. Verify that both endpoints started successfully.
4. Roll back the driver component if verification fails.
5. Launch a first-run page for physical microphone and monitoring selection.

The installer must not silently change the Windows default microphone or edit
game settings. Users select the virtual microphone in each game once. A game
that caches audio devices may need restarting.

## Delivery phases

1. Build the unmodified Simple Audio Sample with pinned VS/SDK/WDK versions.
2. Prove render-to-capture transfer with a generated tone on a disposable test
   machine using test signing.
3. Add the user-mode microphone and effects mixer.
4. Add opt-in UI, device health, levels, and recovery.
5. Add transactional driver install, update, and removal.
6. Run Driver Verifier, HVCI, INF validation, stress tests, and applicable HLK
   playlists on supported Windows versions.
7. Submit the release package through Microsoft Partner Center and ship only
   Microsoft-signed driver binaries.

Production support initially targets Windows 10 22H2 and current Windows 11 on
x64 with Secure Boot and Memory Integrity enabled.
