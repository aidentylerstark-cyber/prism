# Reference Viewers — Firestorm & Black Dragon

We develop Prism by referencing the **actual source** of Firestorm (UI) and Black Dragon
(graphics), not by guessing. Both are LGPL-2.1, the same license as our base.

## Local clones (not committed to Prism)
Shallow clones live outside the repo so we never accidentally publish their code as ours:
```
/home/runner/reference/firestorm      # github.com/FirestormViewer/phoenix-firestorm
/home/runner/reference/black-dragon   # github.com/NiranV/Black-Dragon-Viewer
```

## Verified era (checked 2026-07-12)
All three (Prism base, Firestorm, Black Dragon) sit on the **modern PBR LL base** — each has the
same 21 reflection-probe/PBR shader set. Black Dragon is **not** a pre-PBR fork. Consequence: the
diff between a reference viewer and our base is a *manageable feature delta*, so ports are tractable
rather than archaeology. (BD shader files 233 vs base 219 — a ~14-file delta plus in-file edits.)

## The workflow: diff → understand → port-with-attribution
For any feature:
1. Locate the reference implementation (files below).
2. `diff` the reference file against our base viewer's equivalent to isolate exactly what they
   changed/added — never guess.
3. Adapt into Prism, **keeping LGPL headers** and adding Firestorm/Black Dragon **attribution**
   (About floater credits + file headers). Port-with-attribution, never silent copy-paste.
4. Compile-verify in CI (Windows + macOS) before landing — we cannot build on this Linux box.

## Located reference files

### M2 — UI (Firestorm)
- Radar: `indra/newview/fsradar.{cpp,h}`, `fsfloaterradar.{cpp,h}`, `fspanelradar.{cpp,h}`,
  `fsradarentry.{cpp,h}`, `fsradarlistctrl.{cpp,h}`, `fsradarmenu.{cpp,h}`;
  XUI `skins/*/xui/en/panel_fs_radar.xml`.
- (Area search, contact sets, quick prefs, phototools — to be located per feature.)

### M3 — Graphics (Black Dragon)
- Tonemapping: `app_settings/shaders/class1/deferred/tonemapUtilF.glsl`, `postDeferredTonemap.glsl`
- Ambient occlusion: `app_settings/shaders/class1/deferred/aoUtil.glsl`,
  `class2/deferred/sunLightSSAOF.glsl`
- (Godrays/volumetrics, DOF, shadow tuning — to be located per feature.)

## Attribution obligations
- Preserve each ported file's original LGPL header.
- Credit Firestorm and Black Dragon (and their authors) in the About floater and repo NOTICE.
- Keep Prism's full source public (LGPL + TPV §3.b/§6.a.iv).
