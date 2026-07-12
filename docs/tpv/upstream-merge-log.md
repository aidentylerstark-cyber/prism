# Upstream Merge Log

Prism tracks `secondlife/viewer` via the `upstream` git remote and merges forward on a cadence
(upstream releases roughly monthly — budget a permanent ~15–20% integration tax).

| Date | Prism base | Upstream ref merged | Notes |
|---|---|---|---|
| 2026-07-12 | initial fork | `upstream/main` (last merged `release/2026.02`) | Initial import of the LL viewer as the Prism base. |

## Merge procedure
```bash
git fetch upstream
# review upstream release notes / CHANGELOG for breaking changes
git merge upstream/main            # or the target release branch
# resolve conflicts (Prism changes live in indra/newview/{app_settings/shaders,skins}, branding, .github)
# build in CI (Windows + macOS) and smoke-test before landing
```
Record every merge here with the upstream ref and any notable conflict areas.
