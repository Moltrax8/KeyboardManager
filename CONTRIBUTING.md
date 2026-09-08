# Contributing

Contributions are welcome through GitHub pull requests.

## Development

Requirements are Windows 10 or newer, Visual Studio 2022 with the C++ desktop
workload, CMake 3.24+, Git, and an x64 generator.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Keep business logic outside the Dear ImGui rendering code where practical.
Comments should explain non-obvious decisions rather than restating code.
Changes to dependencies, persisted configuration, input matching, or action
execution should include focused tests and a short rationale in the pull
request.

## Pull requests

- Keep each pull request focused on one change.
- Run the Release build and CTest suite before submitting.
- Do not commit generated builds, installers, private keys, certificates,
  credentials, personal sound files, or configuration data.
- Explain behavioral and compatibility changes in the pull request body.
- Report security vulnerabilities according to `SECURITY.md`, not in a public
  issue.
