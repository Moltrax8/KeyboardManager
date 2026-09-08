# Driver and Release Signing

KeyboardManager has two independent trust requirements. Signing the installer
does not make an unsigned kernel driver loadable, and Microsoft-signing a driver
does not establish the publisher identity of the installer.

## Application and installer

Official releases should Authenticode-sign `KeyboardManager.exe` and the Inno
Setup executable with an identity-validated code-signing certificate and an
RFC 3161 timestamp. SmartScreen also considers reputation, so a valid signature
reduces warnings but cannot guarantee that a new release is never challenged.

Private keys, token PINs, certificate exports, and signing credentials must
never be stored in this repository. CI signing should use a managed signing
service or hardware-backed key with short-lived authentication held in GitHub
environments.

## Kernel driver

Windows 10 version 1607 and later normally require new kernel drivers to be
signed through Microsoft's Hardware Developer Program. The publisher must:

1. Obtain an EV code-signing certificate from a supported certificate
   authority.
2. Complete legal organization or individual verification for Partner Center.
3. Associate the EV certificate with the Hardware Dashboard account.
4. Build the x64 Release package and preserve its PDB symbols.
5. Run the applicable Windows HLK playlists and create an HLKX submission.
6. Sign the submission package, upload it to Partner Center, and download the
   Microsoft-signed result.
7. Verify every catalog and binary signature before release packaging.

Current Microsoft guidance treats attestation signing as a testing path rather
than the retail Windows Update route. Public production releases should plan on
WHCP/HLK certification.

## Development signing

Test certificates are only for dedicated development machines. Test signing
can require Secure Boot to be disabled and is commonly incompatible with
anti-cheat software. Test-signed driver packages must never be attached to a
public GitHub release.

The normal application remains buildable without the WDK. Driver builds use a
pinned Visual Studio 2022, Windows SDK 26100, and WDK 26100 toolchain until the
supported matrix is intentionally changed.

## Maintainer-only steps

Certificate purchase, identity verification, legal enrollment, MFA, and
Partner Center submission approval must be completed by the verified publisher.
Build scripts may automate packaging and verification, but contributors and CI
must not receive the publisher's private signing key.

Microsoft references:

- https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-reqs
- https://learn.microsoft.com/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-
- https://learn.microsoft.com/windows-hardware/test/hlk/
