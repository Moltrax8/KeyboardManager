# Action Architecture

`BindEngine` owns physical-key state, repeat suppression, exact modifier
matching, AltGr handling, pause state, and ordered action submission. It does
not execute operating-system effects.

`ActionExecutor` is the single boundary for actions that affect the system.
The Windows implementation currently owns sound playback, file/application
launching, and simulated media keys. Voice mixing will connect at this boundary
so a sound trigger can feed both monitoring playback and the virtual microphone
without adding audio policy to Raw Input handling.

## Future script actions

Script support must use a dedicated typed action in a newer configuration
version. It must not overload launch paths with raw shell command strings.
The payload should define an explicit interpreter or executable, script path,
argument vector, optional working directory, and timeout.

Execution must leave the window/input thread immediately and use a bounded
worker queue with:

- Explicit maximum concurrency and queue depth.
- Windows Job Objects for process-tree cleanup.
- Non-inherited handles and no implicit `cmd.exe /c` execution.
- Timeout, cancellation, and safe application-shutdown behavior.
- Logs that omit command arguments and output that may contain secrets.

Persisted action names are stable textual identifiers. Unknown actions are
ignored rather than reinterpreted, and unsupported configuration versions
disable saving so an older application cannot destroy future actions.
