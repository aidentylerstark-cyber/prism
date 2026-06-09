# Feasibility Study: Extracting LLPipeline / the Render System into a Library

**Status:** Draft for review
**Scope:** Architectural assessment only — no implementation
**Sibling document:** `doc/compositor-thread-feasibility.md` (the threading split; this
study assumes that work as orthogonal but compatible)

---

## 1. Executive Summary

The stated goal has three parts:

1. `newview` / `secondlife-bin` becomes **headless by default**.
2. A **common renderer interface** exists so different rendering backends can be
   selected at runtime (driver version, hardware, API availability, …).
3. OpenGL state is **removed from `secondlife-bin` and isolated** into a new
   `llpipeline` library.

**These are three different efforts of three very different sizes, and the study's
central finding is that they should not be pursued as one move.** Two of the three
are tractable and are *already in motion*. The third — literally extracting
`LLPipeline` and the draw-pool system into a standalone library — is the hard one,
and its feasibility reduces to a single question:

> **Can we afford to insert an abstract drawable/scene interface that cuts the
> `LLDrawable ↔ LLViewerObject` knot?**

That knot is the crux. `LLDrawable` does not *abstract* a scene object — it *wraps*
one: `lldrawable.h:41` includes `llviewerobject.h` (and `:43` includes
`llappviewer.h`). The draw pools downcast drawables to concrete `LLVO*` types
(terrain→`LLVOSurfacePatch`, water→`LLVOWater`, avatar→`LLVOAvatar`,
tree→`LLVOTree`). `LLSpatialPartition` branches on `getVObj()->getPCode()`. So any
"`llpipeline` library" that preserves that knot must drag `LLViewerObject`,
`LLVOVolume`, `LLVOAvatar`, `LLWorld`, `LLViewerRegion`, `LLViewerTexture`, … with
it — i.e. *most of `newview`* — which defeats the headless-bin goal, because the
app logic still needs its object model. **Without the seam: not feasible as
stated. With the seam: feasible, but a multi-month structural refactor.**

**Verdict, by goal:**

| Goal | Feasibility | Where the work lives |
|---|---|---|
| Backend-agnostic renderer interface | **Feasible, in progress** | inside `llrender` (extend `LLSwapChain`/`LLGPUResource`) |
| Headless by default | **Feasible, modest** | gate GL init in `llappviewer`; null backend |
| Extract `LLPipeline` + draw pools into a library | **Not feasible as stated; feasible only after a scene-interface refactor** | a new `llrender`-adjacent lib, *after* the knot is cut |

**Recommendation:** pursue the backend interface and headless gating now — they sit
in `llrender`, which is already a clean lower-level library, and they deliver most
of the practical value the goal is reaching for. Treat the literal `LLPipeline`
extraction as a *later, optional* phase, explicitly gated on first building the
abstract drawable/scene seam. This is not a redirect away from the goal — it is the
path the in-flight `LLGPUResource` / `LLResourceLease` / `LLSwapChain` work is
already on.

---

## 2. What "extract LLPipeline" actually entails

`LLPipeline` is not a self-contained renderer. It is the **orchestrator of the
viewer's scene-to-screen process**, and it is fused to the viewer's domain model on
both sides:

```
            INBOUND (~100 files reach in)                 OUTBOUND (pipeline reaches out)
  UI / floaters / tools / settings                gAgent, LLWorld, LLViewerRegion,
  LLVO* scene objects   ─── gPipeline ───►         LLViewerCamera, LLSelectMgr,
  startup / app / display      │                   LLEnvironment, LLViewerShaderMgr,
                               ▼                    the entire LLViewerObject / LLVO*
                        draw pools, LLSpatialPartition,   hierarchy, LLFace, ...
                        LLDrawable, LLFace
```

`pipeline.cpp` is ~11.5k lines and `#include`s ~90 headers, the large majority of
them viewer-domain: `llagent.h`, `llselectmgr.h`, `llworld.h`, `llviewerregion.h`,
`llviewerobject.h`, the whole `llvo*` family, plus UI (`llfloatertools.h`,
`llpanelface.h`, `llui.h`, `lltoolmgr.h`). This is the defining fact: **the pipeline
is woven through the viewer's object, world, agent, selection, and UI systems**, not
isolated behind them.

### 2.1 Inbound coupling

Roughly **~100 files** in `newview` reference `gPipeline` / `LLPipeline::`. The
traffic falls into three bands:

- **Clean public API (fine across a boundary).** The dominant calls are drawable
  lifecycle: `markRebuild`, `markMoved`, `markTextured`, plus render-type control
  (`hasRenderType` / `toggleRenderType`) and `createObject`. These are proper
  methods and would survive a library boundary unchanged. This band is the bulk of
  the call *volume*.
- **Leaky static flags (a real problem).** External code reads — and in places
  *writes* — pipeline statics directly: `sRenderingHUDs`, `sImpostorRender`,
  `sShadowRender`, `sUnderWaterRender`, `sReflectionRender`, `sRenderDeferred`,
  `sRenderTransparentWater`, and more. Some writers are UI/feature code (e.g. the
  360-capture floater toggling `sRenderAttachedLights` / `sUseOcclusion`;
  settings-handlers in `llviewercontrol.cpp`). These are an informal shared-state
  contract, not an API.
- **Direct member reach-in (the leakiest).** External code touches pipeline member
  objects and render targets directly: `mReflectionMapManager`, `mHeroProbeManager`,
  internal `LLRenderTarget`s (water reflection, exposure/luminance/scene maps used by
  the GLTF material preview), and even increments instrumentation counters
  (`mTextureMatrixOps`). The GLTF material-preview path chains calls through several
  internal render targets to do its own post-processing.

The clean band is library-ready. The other two bands are an **encapsulation debt**
that must be paid down (accessors / a render-control facade / a state
snapshot-restore API) *before* a boundary can be drawn — independent of any backend
question.

> Note on precision: the counts above are deliberately given as magnitudes. They
> come from grep-style occurrence counting, which is fine for ranking the coupling
> but is not a measurement to base a schedule on.

### 2.2 Outbound coupling — the knot

The harder direction. The render system's core data structures *are* scene-graph
structures:

- **`LLDrawable` wraps `LLViewerObject`.** `lldrawable.h:41` `#include
  "llviewerobject.h"`; the constructor takes `LLViewerObject*`; accessors like
  `getRegion()`, `getTextureEntry()`, `getVOVolume()`, `isAvatar()` forward straight
  to the wrapped object. It is a companion struct, not an abstraction.
- **`LLFace` binds geometry to a viewer object.** Constructed from
  `LLViewerObject*`; `getWorldMatrix()` / `getTextureEntry()` call through to it; it
  downcasts to `LLVOVolume` / `LLVOAvatar` for volume face data.
- **Draw pools are specialized per `LLVO*` subclass.** Terrain reads
  `LLVOSurfacePatch` and `gAgent.getRegion()`; water casts faces to `LLVOWater` and
  reads water settings; the avatar pool pulls in `llvoavatar.h` + `llagent.h`; tree
  reads `getRegion()->mRenderMatrix`.
- **`LLSpatialPartition` choreographs LOD/visibility by object type**, branching on
  `getVObj()->getPCode()` and downcasting to `LLVOVolume` / `LLVOAvatar` /
  `LLControlAvatar`.

The **one genuine abstraction seam already present** is `LLDrawInfo` (the per-batch
descriptor in `llspatialpartition.h`: vertex buffer + texture + model matrix +
shader + counts). Everything at `LLDrawInfo` level and below is backend-shaped data;
everything above it (`LLDrawable`, `LLFace`, `LLSpatialPartition`, the pools'
*setup*) is scene-graph logic. But `LLDrawInfo` is a thin slice — the issuing of
draw calls — not the bulk of the pipeline.

`LLVertexBuffer` already lives in `llrender`, so the lowest layer is in the right
place. The problem is everything between `LLDrawInfo` and `LLViewerObject`.

---

## 3. The backend-agnostic renderer interface

This is the genuinely valuable and *separable* piece — and the part already moving.

### 3.1 Where things stand

`llrender` is **already a clean lower-level library**: it links `llcommon`,
`llimage`, `llmath`, `llwindow`, etc., and pulls in **no `newview` headers**. Where
it needs viewer services it inverts the dependency — e.g. `LLTextureManagerBridge`
(`gTextureManagerBridgep`) lets `llrender` call back into the viewer's texture
manager without compile-time coupling. `llrender.h`'s own header comment states the
intent outright: *"to define an interface for a multiple rendering API
abstraction."* And the in-flight files in this branch — `llgpuresource.h/.cpp`,
`llresourcelease.h`, `llswapchain.h/.cpp` — are the first courses of exactly that
wall, with `LLImageGL` as the first `LLGPUResource` adopter.

So the right home for the renderer interface is **inside / beneath `llrender`**, not
a new `llpipeline` library. The backend question is about *how draw commands and GPU
resources are realized*, which is `llrender`'s job; it is not about *what scene to
draw*, which is `LLPipeline`'s job.

### 3.2 The size of the backend job

OpenGL is **pervasively assumed**, not funneled through one thin layer. There is
partial indirection — enum-translation tables (`LLRender` enums → `GL_*`), RAII
state wrappers (`LLGLEnable`, `LLGLDepthTest`, `LLGLSPipeline`), and the immediate-
mode `gGL` emulator that buffers vertices and caches matrix/texture/light state. But
beneath that:

- Raw `glXxx` calls are scattered across `llrendertarget.cpp` (FBO management),
  `llvertexbuffer.cpp` (VBOs), `llglslshader.cpp` (program/uniform calls),
  `llrender.cpp` (texture units, blend, cull), and `llimagegl.cpp` (textures).
- GL types leak into public signatures (`GLuint`, `GLenum`, `GLint` in
  `LLImageGL` / `LLVertexBuffer` / `LLRender` APIs).
- **Shaders are GLSL-specific end to end.** `LLGLSLShader` calls
  `glAttachShader`/`glLinkProgram`/`glGetUniformLocation`/`glUniform*` directly;
  `LLViewerShaderMgr` (in `newview`) loads `.glsl` files and bakes permutations in as
  preprocessor `#define`s. A non-GL backend needs a different shader story
  (SPIR-V / cross-compilation), and this is the single largest sub-cost.

This is **systematic refactoring, not a rewrite**: route the scattered `glXxx`
through a backend interface, replace GL types in public APIs with opaque resource
handles (the `LLGPUResource` direction), and add a shader-compilation abstraction. A
useful completeness test is to stand up a **null backend** first (see §4) and a
single alternative backend second to validate the interface — before claiming it is
backend-agnostic.

---

## 4. Headless by default

This is the smallest and most independently shippable of the three goals — and a
prerequisite that falls out naturally from the backend interface (a "null" backend
*is* headless).

**Current state is shallow.** `gHeadlessClient` is read in `llappviewer.cpp:3270`,
but the GL bring-up immediately after runs **unconditionally**: `gPipeline.init()`
(`:3397`), `gViewerWindow->initGLDefaults()` (`:3401`), and
`mSwapChain.attachToWindow()` (`:3406`) have no headless guard. The *only* thing
`gHeadlessClient` actually gates is the per-frame `display()` call
(`llappviewer.cpp:1488`; early-out at `llviewerdisplay.cpp:551`). So today "headless"
means "still build the whole GL pipeline, just don't draw each frame."

**True headless-by-default** therefore requires gating *initialization*, not just
the frame loop:

- A null renderer backend selected when no GL context is available/desired, so
  `gPipeline.init()`, shader compilation, and swap-chain attach become no-ops or
  route to stubs.
- Confirming that feature/GPU detection (`llfeaturemanager` via `gGLManager`) and
  `LLViewerShaderMgr` tolerate the null backend.

Critically, this is achievable **without moving `LLPipeline` anywhere** — it is a
gating + null-backend exercise on top of §3. It is the cleanest near-term win and a
good forcing function for the backend interface's completeness.

---

## 5. Why the "one new llpipeline library" framing misleads

The goal pictures: `secondlife-bin` = headless app logic with no GL, and a new
`llpipeline` library = all rendering + all GL, selectable by backend. The trouble is
that **"all rendering" in this codebase is not separable from "the scene/world/
objects."** Moving `LLPipeline` + draw pools + `LLDrawable` + `LLSpatialPartition`
into a library drags `LLViewerObject`, the `LLVO*` hierarchy, `LLWorld`,
`LLViewerRegion`, `LLFace`, and `LLViewerTexture` along — and those depend back on
the pipeline (the ~100 inbound files), producing a **circular dependency** that a
static library boundary cannot express. You would not end up with "thin headless bin
+ render lib"; you would end up with "two libraries that each need the other," i.e.
the monolith with a seam drawn through its middle.

The backend/GL-isolation value the goal is reaching for does **not** require that
move. GL isolation belongs in `llrender` (§3); headless belongs in app-init gating
(§4). The `LLPipeline`-as-library move only becomes coherent *after* the
`LLDrawable ↔ LLViewerObject` knot is cut — and at that point the library boundary
is the abstract scene interface, not "`LLPipeline`."

---

## 6. If the literal extraction is still wanted: the seam

Should the team decide the `LLPipeline` library is a hard requirement, the
unavoidable enabling work is an **abstract drawable/scene interface** that the render
system consumes *instead of* `LLViewerObject`:

1. Define an abstract render-item interface (geometry, transform, material, LOD,
   visibility) that exposes what pools and the spatial partition need, with **no
   `LLViewerObject` knowledge**. `LLDrawInfo` is the model for the batch level; the
   harder part is abstracting what `LLDrawable`/`LLFace` expose.
2. Make `LLViewerObject` / `LLVO*` *implement* (or feed) that interface; invert the
   downcasts in the pools and spatial partition so they call the interface, not
   concrete `LLVO*` types.
3. Pay down the §2.1 inbound debt: replace direct static-flag and member reach-ins
   with a render-control facade and an explicit save/restore state API.
4. Only then move `LLPipeline` + pools + the now-abstract drawable/partition into a
   library that depends on the interface, leaving `LLViewerObject` and the world
   model in `newview`.

Steps 1–3 are independently valuable (they improve the monolith even if the library
is never built) and are where the multi-month cost lives. Step 4 is comparatively
mechanical once 1–3 are done.

---

## 7. Risk register

| # | Risk | Severity | Note |
|---|------|----------|------|
| 1 | Treating the three goals as one move | **High** | Forces the impossible-as-stated extraction to gate the tractable wins; decouple them |
| 2 | `LLDrawable`/`LLFace` wrap `LLViewerObject` directly | **High** | The structural blocker; only an abstract scene interface resolves it |
| 3 | Circular dependency between a render lib and the object model | **High** | A static-lib boundary cannot express it; §5 |
| 4 | Leaky pipeline statics / member reach-ins (~2 of 3 coupling bands) | Medium | Encapsulation debt; payable independently of backend work |
| 5 | GLSL baked through `LLGLSLShader` + `LLViewerShaderMgr` | Medium-High | Largest sub-cost of the backend interface; needs a shader-IR story |
| 6 | "Headless" today only gates `display()`, not init | Medium | Verified; true headless needs init gating + null backend (§4) |
| 7 | Pool specialization per `LLVO*` subtype | Medium | Terrain/water/avatar/tree pools encode object-type logic, not generic geometry |
| 8 | Backend interface declared "done" without a second backend | Medium | Stand up null + one real alternate backend to prove completeness |

---

## 8. Recommended path

**Phase A — Backend interface + GL isolation, inside `llrender` (in progress).**
Continue the `LLGPUResource` / `LLResourceLease` / `LLSwapChain` work: opaque GPU
resource handles, a backend seam under `LLRender`/`LLRenderTarget`/`LLVertexBuffer`,
GL types out of public APIs. This isolates GL *below an interface* without moving
`LLPipeline`.

**Phase B — Null backend + headless-by-default.** Add a null backend and gate
`gPipeline.init()` / shader compile / swap-chain attach on backend availability
(§4). Ship headless-by-default as a standalone capability. Doubles as the
completeness test for Phase A.

**Phase C — Shader abstraction.** Decouple `LLGLSLShader` / `LLViewerShaderMgr` from
GLSL specifics (compilation + permutation strategy). Largest single backend sub-cost;
required before any non-GL backend is real.

**Phase D — Encapsulation debt paydown.** Replace pipeline static-flag and
render-target reach-ins (§2.1 bands 2–3) with accessors / a render-control facade /
state snapshot API. Improves the monolith regardless of later steps.

**Phase E (optional, gated) — The scene interface and the `llpipeline` library.**
Only if a true library split is still required: build the abstract drawable/scene
interface (§6), invert the pool/partition downcasts, then lift `LLPipeline` + pools
+ abstract drawable/partition into a library. This is the multi-month structural
effort; do not start it until A–D have proven the seams.

A–D deliver essentially all of the stated *intent* — selectable backends, GL
isolated out of the bin's logic, headless by default — and are where current work
already points. E is the only part that is "extract `LLPipeline`" literally, and it
should be entered deliberately, with eyes open, as a scene-graph refactor rather than
a packaging change.

---

## 9. Recommendation

**Decouple the goal into its three real efforts and pursue them in dependency
order.** The backend interface and GL isolation (Phase A) and headless-by-default
(Phase B) are feasible, high-value, and already underway in `llrender` — that is the
correct home for them. The literal extraction of `LLPipeline` and the draw pools into
a library is **not feasible as stated**, because the render system's core types wrap
the viewer's object model rather than abstracting it; it becomes feasible only after
an abstract drawable/scene interface cuts the `LLDrawable ↔ LLViewerObject` knot, and
that interface — not "`LLPipeline`" — is the real library boundary.

Frame the program as: *isolate the backend now, go headless now, and treat the
pipeline library as a later option that the scene-interface work unlocks.* That keeps
every near-term step shippable and avoids letting the hardest, least-certain piece
block the value that is already within reach.
