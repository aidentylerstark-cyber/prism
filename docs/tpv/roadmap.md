# Prism Viewer — Roadmap

Fork of `secondlife/viewer` (LGPL-2.1). Goal: Black Dragon-parity-or-better graphics,
Firestorm-parity UX. Honest overall estimate: **6–12 months to a credible public release**
for 1–2 experienced C++ devs, plus a permanent **~15–20% upstream-merge tax** (upstream
releases roughly monthly).

## Verified baseline (checked 2026-07-12)

The **official viewer already ships** much of what made Black Dragon special:
glTF 2.0 PBR materials (metallic/roughness), HDR + auto-exposure + ACES tonemapping,
real-time mirrors (hero reflection probes), SSR, SSAO, and 4-split cascaded shadow maps,
PBR triplanar terrain, and 2K textures. Black Dragon is **OpenGL/GLSL like every SL viewer**
(any "DirectX" claim is false). So the graphics gap is Black Dragon's *remaining deltas* plus
polish — not a renderer rewrite.

## Milestones

### M0 — Fork + clean build base  *(in progress)*
- [x] Fork base: import `secondlife/viewer@main` as Prism base; `upstream` remote for forward-merges.
- [x] Preserve prior workspace contents (archived on branch `archive/web-project-wip`).
- [x] Scaffold `docs/tpv/` + `branding/`.
- [ ] Public GitHub repo (required by TPV §6.a.iv + LGPL) — **blocked on `gh auth login`**.
- [ ] CI green on Windows + macOS producing runnable (unbranded) builds — depends on public repo.
- Estimate: 2–4 weeks (CI + signing is the pain, not the code).

### M1 — Rebrand to Prism
Channel/version string (`VIEWER_CHANNEL`), Windows `.rc` icons, macOS `Info.plist`/bundle ID,
login splash, About floater, update/crash endpoints (point at ours or disable — never phone LL's
update service as the official viewer), honest channel identification at login. No LL trademark
fragments. Estimate: 2–3 weeks.

### M2 — UI (Firestorm parity core)
Port + restyle from Firestorm's LGPL source (with attribution): radar, area search, contact sets,
quick prefs, phototools shell, preferences overhaul, dark-theme skin infra. XUI XML under
`indra/newview/skins/default/xui/en/`, backing `LLFloater`/`LLPanel` in `indra/newview/`,
registered in `llviewerfloaterreg.cpp`. Estimate: 2–4 months.

### M3 — Graphics (Black Dragon parity+)  *(overlaps M2)*
Shaders under `indra/newview/app_settings/shaders/class1..3/deferred/`; pipeline in
`indra/newview/pipeline.cpp`, `lldrawpool*`, environment/settings classes.
1. Baseline audit of the current deferred pipeline (document knobs + quality ceilings).
2. Composable post-process pass framework: exposure → tonemap → color grade → effects.
3. Extra tonemappers (Reinhard/Filmic/Uchimura alongside upstream ACES), color grading, vignette, chromatic aberration, film grain, posterization.
4. AO upgrade: HBAO/GTAO alongside upstream SSAO.
5. Shadow tuning UI (split/bias/blur/resolution); contact-hardening soft shadows.
6. Volumetric lighting (screen-space godrays) integrated with EEP.
7. Photo-tools: high-res snapshot pipeline, full DOF control UI, camera roll, machinima sidebar.
8. Poser (stretch): client-side joint manipulation of own avatar.
Estimate: 3–6 months.

### M4 — TPV Directory registration + release
QA pass, compliance audit against [compliance-checklist.md](compliance-checklist.md), signed
installers, file a "New Application" issue at
[`secondlife/third-party-viewers`](https://github.com/secondlife/third-party-viewers)
(self-certification; requires accounts in good standing + payment-info/age verification).
Estimate: 3–4 weeks + LL review latency.

## Toolchain notes
- Build system: **autobuild** (pip) → CMake; prebuilt 3p libs from `autobuild.xml`;
  compiler flags via `AUTOBUILD_VARIABLES_FILE` (`secondlife/build-variables`, cloned to `/home/runner/build-variables`).
- Windows: VS2022 + CMake + Python 3.7+ + NSIS. macOS: Xcode/clang + CMake + Python 3.7+.
- Channel branding is a CMake-time var: `-DVIEWER_CHANNEL="Prism Release"`.
