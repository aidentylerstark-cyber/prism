# Prism Viewer — Project Documentation

**Prism** is a third-party viewer (TPV) for Second Life, forked from Linden Lab's
open-source viewer (LGPL-2.1). It targets rendering quality matching or exceeding
the Black Dragon viewer and a UX in the spirit of Firestorm.

- **Upstream base:** [`secondlife/viewer`](https://github.com/secondlife/viewer) — tracked via the `upstream` git remote; Prism is based on the `main` (released) line and merges upstream forward on a cadence.
- **License:** LGPL-2.1 (inherited). Our modifications remain source-available.
- **Platforms (initial):** Windows, macOS. Native builds run in CI (this dev box is Linux and lacks the Python/CMake/autobuild toolchain).

## Docs index

| File | Purpose |
|---|---|
| [roadmap.md](roadmap.md) | Milestones M0–M4, workstreams, honest effort estimates |
| [compliance-checklist.md](compliance-checklist.md) | TPV Policy obligations enforced every PR |
| [branding.md](branding.md) | Name/channel/rebrand surface and rules |
| [privacy-policy.md](privacy-policy.md) | Draft published privacy policy (TPV §4.b) |
| [upstream-merge-log.md](upstream-merge-log.md) | Record of upstream merges (the "merge tax") |

## Build reality

Native Windows/macOS builds happen on GitHub Actions runners (`windows-2022` + `macos-14`),
following upstream's `.github/workflows/build.yaml`. macOS release builds require an Apple
Developer ID for notarization; Windows release builds want a code-signing certificate.
See [roadmap.md](roadmap.md) §M0.
