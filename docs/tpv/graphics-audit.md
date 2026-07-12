# M3 Graphics Audit — Base vs Black Dragon (verified deltas)

Baseline audit + execution spec for reaching Black Dragon-parity-or-better. Deltas measured by
diffing `/home/runner/reference/black-dragon` against our base tree
(`indra/newview/app_settings/shaders/`). Black Dragon is LGPL-2.1 — port-with-attribution.

## What our base already ships (no work needed)
glTF 2.0 PBR (metallic/roughness), HDR + auto-exposure + ACES tonemapping, real-time mirrors
(hero reflection probes), SSR, SSAO, 4-split cascaded shadow maps, PBR triplanar terrain, 2K
textures. The gap to BD is its *remaining* deltas below, not a renderer rewrite.

## Black Dragon deltas (measured)

### New shader files = unambiguous BD features
| Feature | Files | Size |
|---|---|---|
| **Volumetric lighting / godrays** | `class1/deferred/volumetricLightF.glsl`, `class3/deferred/volumetricLightF.glsl` | 39 + 109 lines |
| **High-quality DoF** | `class1/deferred/postDeferredHQDoFF.glsl` | 144 lines |
| **Motion blur (velocity buffer)** | `avatarVelocity{F,V}`, `motionBlur{F,V}`, `skinnedVelocity[Alpha]V`, `velocity[Alpha]{F,V}`, `velocityFuncV` (~12 files) | full velocity-G-buffer pipeline |

### Modified shaders = enhancements (diff size vs base)
| File | Δ | Nature |
|---|---|---|
| `class1/deferred/tonemapUtilF.glsl` | **+53 / −0** | **Extra tonemappers** (Reinhard/Filmic/…). Pure addition — lowest-risk port. |
| `class1/deferred/aoUtil.glsl` | **+110 / −0** | **HBAO** alongside base SSAO. Pure addition — low-risk. |
| `class2/deferred/sunLightSSAOF.glsl` | +5 / −0 | AO hook. |
| `class1/deferred/postDeferredF.glsl` | +68 / −53 | Post-process chain rework (wires DoF/effects). Needs per-file diff review. |
| `class1/deferred/shadowUtil.glsl` | +165 / −21 | Shadow tuning/soft shadows. Review needed. |
| `class3/deferred/{reflectionProbeF,screenSpaceReflUtil,multiPointLightF,pointLightF,spotLightF}` | varies | Lighting/reflection tweaks — **may be partly base-version drift, not BD features**; diff each before porting. |
| `class1/deferred/SMAA.glsl` | +1 / −1 | Noise (define tweak) — **skip**. |

> Caveat: `−0` diffs are clean BD feature additions. Files with removals (`postDeferredF`,
> `shadowUtil`, the class3 lighting set) mix BD changes with possible base-version drift — diff
> each individually and port only the BD-authored hunks.

### Verified extracted deltas (from actual diffs)
- **Tonemappers** (`tonemapUtilF.glsl`): BD adds `PBRReinhardToneMapping`, `Uncharted2ToneMapping`,
  and `FilmicToneMapping`, dispatched by a mode selector. Clean, self-contained functions — direct
  port + a `RenderToneMapType` setting and a phototools selector.
- **HBAO** (`aoUtil.glsl`): BD adds `calcHBAmbientOcclusion(vec4 pos, vec3 normal, vec2 pos_screen)`,
  but **BD's own comment flags it: "Shitty ass local HBAO implementation (WIP)… not optimized and
  doesn't put the resulting AO into a texture."** → For parity-or-**better**, do NOT port BD's WIP
  verbatim; implement a clean, texture-backed HBAO (or GTAO) using BD's math only as a starting
  reference. This is the one place "port from BD" is the wrong default.

## C++ / pipeline implications (beyond shaders)
- **Motion blur** is the most invasive: needs a velocity G-buffer target + render pass in
  `indra/newview/pipeline.cpp` and draw-pool wiring, plus per-object prev-frame matrices. Highest
  effort; defer to late M3.
- **Volumetric light** needs a screen-space pass integrated with the EEP sky/atmospherics + a
  toggle/quality setting.
- **DoF/tonemap/HBAO** are mostly shader-side + new `settings.xml`/debug settings and UI toggles
  (feed these into the M2 phototools/quick-prefs floaters).
- Each new feature needs a `RenderXxx` debug setting (default off) and a feature-table entry.

## Phased M3 plan (low-risk / high-value first)
1. **Tonemappers** (`tonemapUtilF` +53/−0) — add Reinhard/Filmic/Uchimura alongside ACES; expose selector. *Cleanest first win.*
2. **HBAO** (`aoUtil` +110/−0, `sunLightSSAOF` +5) — add as an AO-mode option.
3. **Volumetric lighting** (new files) — screen-space godrays + EEP integration + quality setting.
4. **Post-process chain + DoF** (`postDeferredF` rework + `postDeferredHQDoFF`) — composable exposure→tonemap→grade→effects; full DoF control UI.
5. **Shadow tuning** (`shadowUtil` +165) — expose split/bias/blur/resolution; soft shadows.
6. **Extra post effects** — vignette/chromatic aberration/film grain/posterization (from BD post shaders).
7. **Motion blur** (velocity buffer, ~12 files + pipeline.cpp) — most invasive; last.
8. **Skip:** `SMAA.glsl` (base already has it; BD delta is noise).

## TPV compliance
All of the above is **client-side presentation of data the viewer already receives** — no
permissions/DRM, no other-user private data. Compliant. Shared-experience discipline: keep effects
**opt-in** (default off) and never default-disable other creators' intended rendering.

## Method note
Reproduce/extend this audit with:
`diff -r reference/black-dragon/indra/newview/app_settings/shaders workspace/indra/newview/app_settings/shaders`
Port each BD hunk into our tree keeping Black Dragon's LGPL header + attribution.
