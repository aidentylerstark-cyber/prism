# BugSplat Fixes

## Table of Contents

- [Tags](#tags)
- [Issues](#issues)
  - [Startup](#startup)
    - [Intentional Crash on Missing/Invalid XUI Strings File [862904] (404)](#intentional-crash-on-missinginvalid-xui-strings-file-862904-404)
      - Viewer crashes on startup when `strings.xml` is missing, corrupted, or replaced with a non-file entity. Common with manual installations to non-standard drives.
      - **Fix:** Improve error dialog visibility and messaging before intentional crash; retain crash for diagnostics.
      - [QA Pre-Patch](#qa-verification---pre-patch) | [QA Post-Patch](#qa-verification---post-patch)
  - [Rendering](#rendering)
    - [Access Violation in Microsoft OpenGLOn12 Translation Layer [816613] (324)](#access-violation-in-microsoft-openglon12-translation-layer-816613-324)
      - Crash in Microsoft's OpenGL-to-DirectX12 translation layer on systems without native OpenGL drivers (Intel integrated graphics). Third-party bug.
      - **Fix:** Detect `OpenGLOn12` at startup and warn users to install proper GPU drivers.
      - [QA Pre-Patch](#qa-verification---pre-patch-1) | [QA Post-Patch](#qa-verification---post-patch-1)
    - [Access Violation in AMD OpenGL Driver (atio6axx) [795097] (175)](#access-violation-in-amd-opengl-driver-atio6axx-795097-175)
      - Crash in AMD's proprietary OpenGL driver (`atio6axx.dll`) during rendering. Third-party bug, typically resolved by driver updates.
      - **Fix:** Detect AMD GPU and outdated driver versions; display warning with driver download link.
      - [QA Pre-Patch](#qa-verification---pre-patch-2) | [QA Post-Patch](#qa-verification---post-patch-2)
  - [Memory](#memory)
    - [Out of Memory During Mesh Tangent Generation [861157] (264)](#out-of-memory-during-mesh-tangent-generation-861157-264)
      - `std::bad_alloc` in MikkTSpace tangent generation for complex meshes. Viewer crashes instead of gracefully degrading.
      - **Fix:** Skip tangent generation on OOM and log warning; add size check to reject oversized meshes.
      - [QA Pre-Patch](#qa-pre-patch-verification) | [QA Post-Patch](#qa-verification---post-patch-3)
  - [Voice](#voice)
    - [WebRTC Audio Thread Crash on macOS During Device Switching [858204] (174)](#webrtc-audio-thread-crash-on-macos-during-device-switching-858204-174)
      - Race condition in WebRTC `AudioDeviceMac::StopPlayout()` when switching audio devices on macOS. Intentional assertion crash in third-party library.
      - **Fix:** Debounce device switching with 100ms cooldown; check `Playing()` state before calling `StopPlayout()`.
      - [QA Pre-Patch](#qa-pre-patch-verification-1) | [QA Post-Patch](#qa-verification---post-patch-4)

## Tags

- `#Startup` - Issues during application initialization
- `#Rendering` - Graphics and display issues
- `#Networking` - Network communication issues
- `#Inventory` - Inventory system issues
- `#Avatar` - Avatar-related issues
- `#Media` - Media playback issues
- `#UI` - User interface issues
- `#Memory` - Memory management issues
- `#Threading` - Concurrency issues
- `#Voice` - Voice chat and WebRTC issues

## Summary Table

| Bug | Count | User Impact | Business Impact | Security | Fix Confidence | Priority | Release Vehicle |
|-----|-------|-------------|-----------------|----------|----------------|----------|-----------------|
| [XUI Strings File Missing [862904]](#intentional-crash-on-missinginvalid-xui-strings-file-862904-404) | 404 | 3 (High) | 2 (Medium) | N/A | 2 | | |
| [OpenGLOn12 Crash [816613]](#access-violation-in-microsoft-openglon12-translation-layer-816613-324) | 324 | 3 (High) | 2 (Medium) | N/A | 1 | | |
| [Mesh Tangent OOM [861157]](#out-of-memory-during-mesh-tangent-generation-861157-264) | 264 | 3 (High) | 2 (Medium) | N/A | 3 | | |
| [AMD Driver Crash [795097]](#access-violation-in-amd-opengl-driver-atio6axx-795097-175) | 175 | 2 (Medium) | 2 (Medium) | N/A | 1 | | |
| [WebRTC macOS Audio [858204]](#webrtc-audio-thread-crash-on-macos-during-device-switching-858204-174) | 174 | 3 (High) | 2 (Medium) | N/A | 2 | | |

## Issues

### Startup

#### Intentional Crash on Missing/Invalid XUI Strings File [862904] (404)

**Functional Area:** `#Startup`

**Cause of Crash:**

This crash is an **intentional termination** triggered by the `LL_ERRS()` macro in `LLAppViewer::initStrings()` when the viewer cannot locate or validate the required localization file `strings.xml`. The `LL_ERRS()` macro deliberately triggers an access violation (0xC0000005) by writing to address `0xDEADBEEFDEADBEEF` as a controlled crash mechanism.

The crash occurs during early startup when:
1. `findSkinnedFilenameBaseLang()` locates a path for `strings.xml`
2. `LLFile::stat()` succeeds on the path
3. The returned file mode is neither a regular file (`S_ISREG` false) nor a directory (`S_ISDIR` false)
4. This indicates the path resolves to a special file type (e.g., device node, named pipe, or corrupted filesystem entry)

**Platform-Specific Notes:**
- **Windows (this crash):** SEH exception 0xC0000005 (Access Violation) is triggered intentionally by the `LLERROR_CRASH` macro
- **Common pattern:** Both observed instances involve non-standard installation paths on secondary drives:
  - `F:\sl\sl_1\skins\default\xui\en\strings.xml`
  - `D:\Second Life\skins\default\xui\en\strings.xml`
- This strongly suggests users manually installing or copying the viewer to non-system drives, bypassing the standard installer
- Windows filesystem corruption or antivirus quarantine could cause a file to report anomalous mode bits

**Files and Functions Affected:**
- `indra/newview/llappviewer.cpp`: `LLAppViewer::initStrings()` (lines 3021-3095)
- `indra/llcommon/llfile.cpp`: `LLFile::isfile()`, `LLFile::stat()`, `LLFile::getattr()`
- `indra/llcommon/llerror.h`: `LL_ERRS()` macro, `LLERROR_CRASH` macro

**User-Facing Experience:**

Users experience an immediate crash on startup before the viewer window fully appears. The crash occurs during early initialization, so:
- No login screen is displayed
- No error dialog may be visible (depending on timing)
- The crash reporter (BugSplat) activates with the description shown

**Likely Triggers:**
- **Primary suspect:** Manual installation to non-standard drive locations (D:\, F:\, etc.) without using the official installer
- Corrupted installation (incomplete download, disk errors, interrupted update)
- Antivirus software quarantining or modifying viewer files
- Running viewer from a network drive or unusual filesystem
- Manually copying viewer files without proper installation
- Symbolic links or junction points that resolve incorrectly
- Steam Workshop or third-party mod managers interfering with installation

**User Impact Rating:** 3 (High)
- **Justification:** Complete startup failure prevents all functionality. However, the crash is deterministic and occurs early, so no user data is at risk. Reinstallation resolves the issue in most cases.

**Business Impact Rating:** 2 (Medium)
- **Justification:** Users encountering this crash cannot use the product until reinstalling. Support burden increases. However, the error message provides actionable guidance and the crash is caused by external factors (installation corruption) rather than code defects.

**Security Impact:** N/A
- This crash is not exploitable. The intentional crash mechanism prevents the viewer from operating in an inconsistent state. No user data is exposed and the crash cannot be triggered remotely.

**Reproduction Steps:**

1. Create a corrupted installation scenario:
   - Install viewer to a non-standard path (e.g., `F:\sl\sl_1\`)
   - Rename `strings.xml` to a directory, OR
   - Replace `strings.xml` with a symbolic link to a non-existent target, OR
   - Use filesystem tools to create a special file at that path
2. Launch the viewer
3. **Expected:** Crash with access violation at `LLAppViewer::initStrings`
4. **Determinism:** This crash is deterministic given the precondition of an invalid `strings.xml` path

**Suggested Fix:**

The current behavior is actually correct defensive programming - the viewer cannot operate without valid localization strings. However, the user experience can be improved:

1. **Improve the error dialog visibility** - Ensure `LLError::LLUserWarningMsg::showMissingFiles()` completes and displays before the crash occurs
2. **Add recovery guidance** - Enhance the error message to specify which file failed and suggest reinstallation
3. **Consider graceful exit** - Replace `LL_ERRS()` with a user-facing dialog followed by `exit(1)` to avoid crash reporter activation for known-unrecoverable states

**Proposed Implementation:**

```cpp
// In LLAppViewer::initStrings(), replace lines 3050-3055 with:

        // initial check to make sure files are there failed
        gDirUtilp->dumpCurrentDirectories(LLError::LEVEL_WARN);
        
        std::string error_msg = "Second Life cannot start because a required file is missing or corrupted.\n\n"
            "File: " + (strings_path_full.empty() ? strings_file : strings_path_full) + "\n"
            "Problem: " + crash_reason + "\n\n"
            "Please reinstall the viewer from https://secondlife.com/support/downloads/\n"
            "If this problem persists, contact https://support.secondlife.com";
        
        LLError::LLUserWarningMsg::show(error_msg, LLError::LLUserWarningMsg::ERROR_MISSING_FILES);
        
        // Give the dialog time to display, then exit cleanly instead of crashing
        LL_WARNS() << "Viewer failed to open localization files. " << crash_reason << LL_ENDL;
        
        // Use LL_ERRS to ensure proper crash reporting for diagnostic purposes
        LL_ERRS() << "Viewer failed to open some of localization and UI files."
            << " " << crash_reason << "." << LL_ENDL;
```

**Note:** The `LL_ERRS()` behavior should be retained to maintain crash reporting for diagnostic purposes. The improvement is in the user-facing messaging.

**Fix Confidence:** 2 (Low-Medium)
- **Justification:** This crash represents correct behavior for an unrecoverable startup failure. The "fix" is improving user messaging, not preventing the crash. The root cause is installation corruption which is outside the viewer's control. The suggested changes improve UX but do not address why the user's installation became corrupted.

**QA Verification - Pre-Patch:**
1. Install viewer to a test directory
2. Rename `skins/default/xui/en/strings.xml` to `strings.xml.bak`
3. Create directory `skins/default/xui/en/strings.xml/`
4. Launch viewer
5. **Verify:** Crash occurs with BugSplat report matching Stack ID pattern

**QA Verification - Post-Patch:**
1. Repeat pre-patch steps
2. **Verify:** User-facing error dialog appears with enhanced message before crash
3. **Verify:** Error message includes the problematic file path
4. **Verify:** Crash still occurs (for diagnostic data collection)
5. **Verify:** Restoring `strings.xml` allows normal startup

### Rendering

#### Access Violation in Microsoft OpenGLOn12 Translation Layer [816613] (324)

**Functional Area:** `#Rendering`

**Cause of Crash:**

This crash occurs entirely within Microsoft's `OpenGLOn12.dll`, which is Windows' OpenGL-to-DirectX 12 translation layer. The stack trace shows only:
```
OpenGLOn12
OpenGLOn12
```

This indicates the crash happens deep inside Microsoft's compatibility shim with no Second Life viewer code on the stack. `OpenGLOn12.dll` is automatically loaded by Windows when:
1. An application requests legacy OpenGL rendering
2. The GPU driver doesn't provide native OpenGL support (common with Intel integrated graphics)
3. Windows falls back to translating OpenGL calls to DirectX 12

**Platform-Specific Notes:**
- **Windows only:** `OpenGLOn12.dll` is a Windows system component
- **SEH Exception 0xC0000005 (Access Violation):** The crash is a null pointer dereference or invalid memory access within Microsoft's translation layer
- **Common hardware:** Intel UHD/Iris integrated graphics without proper OpenGL ICD (Installable Client Driver)
- **Driver dependency:** This occurs when the GPU vendor's OpenGL driver is missing, outdated, or disabled, forcing Windows to use its fallback OpenGL implementation

**Files and Functions Affected:**

This crash occurs in **third-party system code** (`OpenGLOn12.dll`), not in Second Life viewer code. However, the viewer code that triggers the OpenGL calls is likely in:
- `indra/llwindow/llwindowwin32.cpp`: `LLWindowWin32::switchContext()` - OpenGL context creation
- `indra/llrender/llgl.cpp`: `LLGLManager::initWGL()` - WGL extension initialization
- Any rendering code path that issues OpenGL draw calls

**User-Facing Experience:**

Users experience a hard crash at unpredictable times. The crash may occur:
- During viewer startup when creating the OpenGL context
- During normal use when issuing specific OpenGL commands
- When changing graphics settings
- When the scene complexity triggers certain OpenGL code paths

The viewer window may briefly appear before crashing, or the crash may occur before any UI is visible. No specific user action reliably triggers the crash.

**Likely Triggers:**
- Systems with Intel integrated graphics (UHD 620, UHD 630, Iris Xe, etc.)
- Missing or outdated Intel graphics drivers
- Windows systems where the GPU vendor's OpenGL ICD is not installed
- Systems where DirectX 12 is the primary graphics API but legacy OpenGL support is poor
- Virtual machines or remote desktop sessions with limited GPU passthrough
- Recent Windows updates that may have affected `OpenGLOn12.dll`

**User Impact Rating:** 3 (High)
- **Justification:** The crash is severe (complete application termination) and affects a significant user segment (324 occurrences). However, the crash is intermittent and workarounds exist (driver updates, disabling hardware acceleration). Users with dedicated NVIDIA/AMD GPUs are unaffected.

**Business Impact Rating:** 2 (Medium)
- **Justification:** This crash affects users with specific hardware configurations (primarily Intel integrated graphics). These users represent a notable but not majority segment. The root cause is in Microsoft's code, limiting Linden Lab's ability to fix directly. Support burden increases for driver-related issues.

**Security Impact:** N/A
- This crash occurs in Microsoft system code and is not exploitable from the viewer's perspective. The crash cannot be triggered remotely or used to compromise user data.

**Reproduction Steps:**

1. **Environment Setup:**
   - Use a Windows 11 system with Intel integrated graphics (UHD or Iris series)
   - Uninstall or disable Intel's OpenGL driver (force Windows to use `OpenGLOn12.dll`)
   - Alternatively, use a virtual machine with limited GPU support
2. Launch Second Life Viewer
3. **Expected:** Crash may occur during startup or during rendering
4. **Determinism:** This crash is **intermittent** - it depends on specific OpenGL command sequences that trigger bugs in `OpenGLOn12.dll`. Not all affected systems will crash consistently.

**Suggested Fix:**

Since the crash is in Microsoft system code, the viewer cannot directly fix the bug. However, several mitigations are possible:

1. **Detection and Warning:** Detect when `OpenGLOn12.dll` is the active OpenGL implementation and warn users that their system may be unstable, recommending driver updates.

2. **OpenGL Command Sanitization:** Audit OpenGL usage for commands known to be problematic with `OpenGLOn12` and use alternative approaches where possible.

3. **Fallback Rendering Mode:** Implement a "safe mode" that avoids OpenGL features known to be buggy in `OpenGLOn12`.

4. **Hardware Blocklist:** Add systems using `OpenGLOn12` to a soft blocklist that recommends users update drivers before continuing.

**Proposed Implementation:**

```cpp
// In indra/llrender/llgl.cpp, add detection during initGL():

bool LLGLManager::initGL()
{
    // ... existing initialization ...
    
    // Detect OpenGLOn12 usage
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    
    if (renderer && vendor)
    {
        std::string renderer_str(renderer);
        std::string vendor_str(vendor);
        
        // OpenGLOn12 typically reports "D3D12" or "Microsoft" in the renderer string
        if (renderer_str.find("D3D12") != std::string::npos ||
            renderer_str.find("Microsoft Basic") != std::string::npos ||
            vendor_str.find("Microsoft") != std::string::npos)
        {
            mIsOpenGLOn12 = true;
            LL_WARNS("RenderInit") << "Detected Microsoft OpenGLOn12 fallback renderer. "
                << "This may cause instability. Please update your graphics drivers." << LL_ENDL;
            
            // Show user warning
            LLNotificationsUtil::add("OpenGLOn12Warning");
        }
    }
    
    // ... rest of initialization ...
}
```

**Alternative Recommendation:**

Since this is a Microsoft bug, the most effective solution is **user education**:
- Add a Knowledge Base article about updating Intel graphics drivers
- Detect `OpenGLOn12` and display a prominent warning with driver download links
- Consider refusing to run on `OpenGLOn12` with a clear error message directing users to install proper drivers

**Fix Confidence:** 1 (Low)
- **Justification:** The crash is in third-party Microsoft code that Linden Lab cannot modify. The suggested mitigations are detection and avoidance strategies, not fixes for the underlying bug. The actual fix must come from Microsoft or from users updating their GPU drivers to get a proper OpenGL implementation.

**QA Verification - Pre-Patch:**
1. Set up a Windows 11 VM with no GPU passthrough (or Intel integrated graphics with drivers removed)
2. Verify that `OpenGLOn12.dll` is being used (check via Process Explorer or similar)
3. Launch viewer repeatedly
4. **Verify:** Crashes occur intermittently with `OpenGLOn12` on the stack

**QA Verification - Post-Patch:**
1. Repeat pre-patch setup
2. Launch viewer
3. **Verify:** Warning dialog appears indicating `OpenGLOn12` detection
4. **Verify:** Crash may still occur (mitigation is user awareness, not crash prevention)
5. Install proper Intel graphics drivers
6. **Verify:** Warning no longer appears and stability improves

#### Access Violation in AMD OpenGL Driver (atio6axx) [795097] (175)

**Functional Area:** `#Rendering`

**Cause of Crash:**

This crash occurs entirely within AMD's OpenGL driver (`atio6axx.dll`). The stack trace shows only:
```
atio6axx
```

This indicates the crash happens deep inside AMD's proprietary OpenGL implementation with no Second Life viewer code on the stack. The `atio6axx.dll` library is AMD's 64-bit OpenGL Installable Client Driver (ICD) for Radeon graphics cards.

**Platform-Specific Notes:**
- **Windows only:** `atio6axx.dll` is AMD's Windows OpenGL driver
- **SEH Exception 0xC0000005 (Access Violation):** A null pointer dereference or invalid memory access within AMD's driver code
- **Common hardware:** AMD Radeon RX series, Radeon Pro, and integrated Radeon graphics
- **Driver dependency:** This crash typically occurs with:
  - Outdated AMD Adrenalin drivers
  - Corrupted driver installations
  - Driver/OS version mismatches
  - Specific OpenGL command sequences that trigger driver bugs

**Files and Functions Affected:**

This crash occurs in **third-party vendor code** (`atio6axx.dll`), not in Second Life viewer code. However, the OpenGL commands that trigger the crash originate from:
- `indra/llrender/llgl.cpp` - OpenGL state management
- `indra/llrender/llrender.cpp` - Rendering pipeline
- `indra/llrender/llvertexbuffer.cpp` - Vertex buffer management
- `indra/llwindow/llwindowwin32.cpp` - Windows OpenGL context management

**User-Facing Experience:**

Users experience a hard crash at unpredictable times. The crash may occur:
- During viewer startup when initializing OpenGL
- During scene rendering when complex geometry is displayed
- When changing graphics settings
- When loading new regions or mesh content

The viewer window may freeze briefly or crash immediately with no warning. AMD users with older drivers are most affected.

**Likely Triggers:**
- AMD Radeon graphics cards (RX 400 series through RX 7000 series)
- Outdated AMD Adrenalin drivers
- High graphics settings (especially anti-aliasing, shadows, advanced lighting)
- Complex scenes with many mesh objects
- Specific shader combinations that expose driver bugs
- Windows 10/11 with certain AMD driver versions

**User Impact Rating:** 2 (Medium)
- **Justification:** While the crash is severe (complete application termination), it affects only AMD GPU users and is often resolved by driver updates. With 175 occurrences, it represents a smaller subset than other crashes. Workarounds exist (driver updates, graphics settings reduction).

**Business Impact Rating:** 2 (Medium)
- **Justification:** AMD represents a significant but not majority GPU market share. Users can often self-remedy by updating drivers. Support burden is limited as the issue is known to be driver-related.

**Security Impact:** N/A
- This crash occurs in AMD's proprietary driver code and is not exploitable from the viewer's perspective. The crash cannot be triggered remotely or used to compromise user data.

**Reproduction Steps:**

1. **Environment Setup:**
   - Use a Windows system with AMD Radeon graphics
   - Install an older version of AMD Adrenalin drivers (or use current drivers if known buggy)
2. Launch Second Life Viewer
3. Enable high graphics settings (Ultra or High with shadows enabled)
4. Navigate to a mesh-heavy region
5. **Expected:** Crash may occur during rendering
6. **Determinism:** This crash is **intermittent** - it depends on specific OpenGL command sequences that trigger driver bugs. Not all AMD systems will crash.

**Suggested Fix:**

Since the crash is in AMD driver code, the viewer cannot directly fix the bug. Possible mitigations include:

1. **Detection and Warning:** Detect AMD GPU and driver version at startup, warn users if running a known-buggy driver version.

2. **AMD-specific OpenGL Workarounds:** Some OpenGL features or command sequences are known to be buggy on AMD. Implement AMD-specific code paths that avoid problematic patterns.

3. **Graphics Settings Profile:** Create an "AMD Compatible" graphics preset that disables features known to cause issues with AMD drivers.

4. **Driver Version Blocklist:** Maintain a list of known-buggy AMD driver versions and display a warning if detected.

**Proposed Implementation:**

```cpp
// In indra/llrender/llgl.cpp, add AMD detection during initGL():

bool LLGLManager::initGL()
{
    // ... existing initialization ...
    
    // Detect AMD GPU and driver issues
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    
    if (vendor && renderer)
    {
        std::string vendor_str(vendor);
        std::string renderer_str(renderer);
        
        if (vendor_str.find("ATI") != std::string::npos ||
            vendor_str.find("AMD") != std::string::npos ||
            vendor_str.find("Advanced Micro Devices") != std::string::npos)
        {
            mIsAMD = true;
            LL_INFOS("RenderInit") << "Detected AMD GPU: " << renderer_str << LL_ENDL;
            
            // Parse driver version and check against known-buggy versions
            // AMD OpenGL version strings typically include driver version info
            if (version)
            {
                std::string version_str(version);
                // Check for known buggy driver versions
                // (Maintain a list based on crash reports)
                
                if (isKnownBuggyAMDDriver(version_str))
                {
                    LL_WARNS("RenderInit") << "Detected potentially unstable AMD driver version. "
                        << "Please update your graphics drivers from https://www.amd.com/support" 
                        << LL_ENDL;
                    LLNotificationsUtil::add("AMDDriverWarning");
                }
            }
        }
    }
    
    // ... rest of initialization ...
}
```

**Alternative Recommendation:**

Since this is an AMD driver bug, the most effective solutions are:
- **Knowledge Base article:** Document the issue and recommend driver updates
- **Automatic driver detection:** Detect old/buggy AMD drivers and show a warning
- **Reduce OpenGL complexity:** On AMD, automatically disable features known to trigger driver bugs (e.g., certain MSAA modes, specific shader features)

**Fix Confidence:** 1 (Low)
- **Justification:** The crash is in third-party AMD code that Linden Lab cannot modify. The suggested mitigations are detection and avoidance strategies, not fixes for the underlying bug. The actual fix must come from AMD or from users updating their GPU drivers.

**QA Verification - Pre-Patch:**
1. Set up a Windows system with AMD Radeon graphics and an older driver version
2. Launch viewer with high graphics settings
3. Navigate to complex scenes
4. **Verify:** Crashes occur intermittently with `atio6axx` on the stack

**QA Verification - Post-Patch:**
1. Repeat pre-patch setup
2. Launch viewer
3. **Verify:** Warning dialog appears indicating AMD driver detection (if driver is in blocklist)
4. **Verify:** Crash may still occur (mitigation is user awareness, not crash prevention)
5. Update to latest AMD Adrenalin drivers
6. **Verify:** Warning no longer appears and stability improves

### Memory

#### Out of Memory During Mesh Tangent Generation [861157] (264)

**Functional Area:** `#Memory` `#Rendering`

**Cause of Crash:**

This crash occurs when the viewer runs out of memory while generating tangent data for mesh assets using the MikkTSpace algorithm. The crash is an **intentional termination** triggered by `LL_ERRS()` after catching a `std::bad_alloc` exception.


The crash path is:
1. A mesh LOD is received from the server via `LLMeshRepoThread::lodReceived()`
2. The mesh data is unpacked via `LLVolume::unpackVolumeFaces()`
3. Cache optimization is triggered via `LLVolume::cacheOptimize()`
4. Tangent generation begins in `LLVolumeFace::cacheOptimize()` with `gen_tangents=true`
5. `MikktData` is constructed, allocating `p`, `n`, `tc`, `w`, `t` vectors sized to `face->mNumIndices`
6. `ctx.genTangSpace()` (MikkTSpace library) performs internal allocations
7. A `std::bad_alloc` is thrown due to memory exhaustion
8. The catch block calls `LL_ERRS()` which deliberately crashes

The stack trace shows exception handling frames (`catch$46`, `CallSettingFrame_LookupContinuationIndex`, `__FrameHandler4::CxxCallCatchBlock`) confirming the `std::bad_alloc` was caught and the crash is intentional.

**Platform-Specific Notes:**
- **Windows (this crash):** SEH exception 0xC0000005 triggered intentionally by `LL_ERRS()` after catching `std::bad_alloc`
- The crash occurs on a **background thread** (`LL::ThreadPoolBase::run`) in the mesh loading thread pool
- Memory pressure may be exacerbated by:
  - 32-bit address space limitations (if running 32-bit viewer)
  - System-wide memory pressure from other applications
  - Multiple simultaneous mesh loading operations
  - Exceptionally large or complex mesh assets

**Files and Functions Affected:**
- `indra/llmath/llvolume.cpp`: 
  - `LLVolumeFace::cacheOptimize()` (line 5665-5800) - crash location
  - `MikktData` constructor (line 5576-5630) - initial allocation
  - `LLVolume::unpackVolumeFacesInternal()` (line 2321)
  - `LLVolume::cacheOptimize()` (line 2763)
- `indra/newview/llmeshrepository.cpp`:
  - `LLMeshRepoThread::lodReceived()` (line 2374)
  - `LLMeshLODHandler::processLod()` (line 3811)

**User-Facing Experience:**

Users experience a crash during normal gameplay when:
- Teleporting to a region with many mesh objects
- Approaching areas with complex mesh content (avatars, buildings, vehicles)
- Loading a mesh-heavy scene after login

The crash occurs on a background thread, so the user may see the viewer freeze momentarily before crashing. There is no warning dialog. The crash is more likely when:
- The system has low available RAM
- Many mesh objects are being loaded simultaneously
- Mesh objects have high face counts or complex geometry

**Likely Triggers:**
- Entering regions with dense mesh content
- Multiple avatars with mesh attachments nearby
- Rezzing or viewing high-polygon mesh objects
- Systems with limited RAM (8GB or less)
- Running other memory-intensive applications alongside Second Life

**User Impact Rating:** 3 (High)
- **Justification:** The crash completely terminates the viewer and occurs during normal gameplay without user provocation. With 264 occurrences, it affects a significant number of users. However, it is intermittent and depends on specific mesh content and memory conditions.

**Business Impact Rating:** 2 (Medium)
- **Justification:** Users may avoid certain regions known to cause crashes, reducing engagement. The crash occurs during content consumption, not content creation, limiting economic impact. Support tickets may increase for "random crashes."

**Security Impact:** N/A
- This crash cannot be exploited for security purposes. The memory exhaustion is a resource limit, not a buffer overflow or similar vulnerability. A malicious mesh could potentially be crafted to trigger OOM conditions, but this would only cause a denial-of-service to the individual viewer, not a security breach.

**Reproduction Steps:**

1. **Environment Setup:**
   - Use a system with limited RAM (8GB or less) or artificially constrain memory
   - Ensure the viewer is loading many mesh objects
2. Teleport to a mesh-heavy region (e.g., a busy shopping area or event venue)
3. Move around to trigger mesh loading for nearby objects
4. **Expected:** Crash may occur during mesh loading, especially if memory is constrained
5. **Determinism:** This crash is **intermittent** - it depends on memory availability at the moment of allocation. High-polygon meshes increase likelihood.

**QA Pre-Patch Verification:**
1. Set Windows virtual memory limits or use a tool to limit available RAM
2. Create a test region with many high-polygon mesh objects
3. Teleport into the region
4. **Verify:** Crash occurs with BugSplat report showing `cacheOptimize` and `genTangSpace` in stack

**Suggested Fix:**

The current behavior crashes the entire viewer when a single mesh fails to allocate memory for tangent generation. A more graceful approach would be to:

1. **Fail the individual mesh, not the viewer:** Instead of `LL_ERRS()` (which crashes), log a warning and skip tangent generation for that face, allowing the mesh to render without tangents (visually degraded but functional).

2. **Add pre-allocation size checks:** Before allocating MikktData vectors, check if the required memory is reasonable and skip tangent generation for meshes that exceed a threshold.

3. **Implement progressive loading:** Limit the number of simultaneous mesh tangent generation operations to reduce peak memory usage.

**Proposed Implementation:**

```cpp
// In indra/llmath/llvolume.cpp, LLVolumeFace::cacheOptimize()
// Replace the try-catch block around ctx.genTangSpace():

bool LLVolumeFace::cacheOptimize(bool gen_tangents)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VOLUME;
    llassert(!mOptimized);
    mOptimized = true;

    if (gen_tangents && mNormals && mTexCoords)
    {
        // Check if mesh is too large for tangent generation
        constexpr U32 MAX_INDICES_FOR_TANGENTS = 500000; // ~500k indices
        if (mNumIndices > MAX_INDICES_FOR_TANGENTS)
        {
            LL_WARNS("Volume") << "Skipping tangent generation for oversized mesh: " 
                << mNumIndices << " indices (limit: " << MAX_INDICES_FOR_TANGENTS << ")" << LL_ENDL;
            gen_tangents = false;
        }
        else
        {
            MikktData data(this);
            mikk::Mikktspace ctx(data);
            try
            {
                ctx.genTangSpace();
            }
            catch (std::bad_alloc&)
            {
                // Log warning but continue without tangents instead of crashing
                LL_WARNS("Volume") << "Out of memory during tangent generation for mesh with " 
                    << mNumIndices << " indices. Mesh will render without tangents." << LL_ENDL;
                LLError::LLUserWarningMsg::showOutOfMemory();
                // Continue without tangents - mesh will still render, just without proper tangent-based effects
                return true; // Don't return false, allow mesh to be used
            }
            
            // ... rest of tangent processing ...
        }
    }
    
    // ... rest of cache optimization ...
}
```

**Additional Recommendations:**
1. Add memory usage tracking to detect when the viewer is approaching memory limits
2. Implement a mesh complexity budget system that degrades quality before running out of memory
3. Consider lazy tangent generation - only generate tangents when the mesh is actually rendered with materials that need them

**Fix Confidence:** 3 (Medium-High)
- **Justification:** The fix directly addresses the crash by converting a fatal error into a graceful degradation. The mesh will still render, just without tangent-based effects (normal mapping, etc.). However, this treats the symptom (crash on OOM) rather than the root cause (excessive memory usage). A complete solution would require memory budgeting across the mesh loading system.

**QA Verification - Post-Patch:**
1. Repeat pre-patch reproduction steps with constrained memory
2. **Verify:** Instead of crashing, the viewer logs a warning about skipping tangent generation
3. **Verify:** The mesh renders (possibly with visual artifacts due to missing tangents)
4. **Verify:** The viewer continues to function normally
5. Test with normal memory conditions to ensure tangent generation still works when memory is available

### Voice

#### WebRTC Audio Thread Crash on macOS During Device Switching [858204] (174)

**Functional Area:** `#Voice` `#Threading`

**Cause of Crash:**

This crash occurs within the WebRTC library (`libllwebrtc.dylib`) on macOS when the audio device module encounters an invalid internal state during device switching or teardown. The crash is an **intentional termination** triggered by WebRTC's internal assertion system.

The stack trace shows:
1. `LLWebRTCImpl::workerDeployDevices()` calls `mDeviceModule->StopPlayout()` to stop audio playback before switching devices
2. Inside `webrtc::AudioDeviceMac::StopPlayout()`, the macOS-specific audio device code attempts to clean up a `PlatformThread`
3. The `PlatformThread::~PlatformThread()` destructor detects an invalid state (thread still running or in an unexpected state)
4. This triggers `webrtc::webrtc_checks_impl::UnreachableCodeReached()`, which is a WebRTC assertion mechanism
5. `WriteFatalLog()` intentionally crashes with `SIGSEGV` at address `0x0` (null pointer dereference) as a controlled termination

**Platform-Specific Notes:**
- **macOS only:** The `AudioDeviceMac` class is Apple-specific. Windows uses `AudioDeviceWindowsCore` and Linux uses `AudioDeviceLinuxALSA` or PulseAudio
- **SIGSEGV at 0x0:** This is a deliberate null pointer dereference in `WriteFatalLog()` to generate a crash dump
- **Threading issue:** The crash occurs when the audio playout thread is not in the expected state during cleanup, possibly due to:
  - Race condition between device switching and audio thread termination
  - macOS Core Audio callback still active when cleanup begins
  - Rapid successive device changes overwhelming the state machine

**Files and Functions Affected:**

The crash originates from viewer code in `indra/llwebrtc/llwebrtc.cpp`:
- `LLWebRTCImpl::workerDeployDevices()` (line 459) - calls `StopPlayout()` at line 486
- `LLWebRTCImpl::deployDevices()` (line 680) - posts task to worker thread
- `LLWebRTCImpl::terminate()` (line 354) - also calls `StopPlayout()` during shutdown

The actual crash occurs in WebRTC library code (`libllwebrtc.dylib`):
- `webrtc::AudioDeviceMac::StopPlayout()` - macOS audio device implementation
- `webrtc::PlatformThread::~PlatformThread()` - thread management
- `webrtc::webrtc_checks_impl::UnreachableCodeReached()` - assertion failure handler

**User-Facing Experience:**

Users experience a crash during voice-related operations on macOS:
- Changing audio input/output devices while voice is active
- Connecting to or disconnecting from voice chat
- Plugging/unplugging USB headsets or switching Bluetooth audio devices
- Opening Preferences and changing audio device selections
- Viewer shutdown while voice is active

The crash occurs without warning, terminating the viewer entirely. Users may lose unsaved work (e.g., unsent chat messages, in-progress builds).

**Likely Triggers:**
- Rapidly switching audio devices in System Preferences or viewer Preferences
- Unplugging/plugging USB audio devices during voice chat
- Bluetooth audio device disconnection (e.g., AirPods running out of battery)
- System audio device changes triggered by other applications
- Closing laptop lid (triggering audio device change on some macOS configurations)

**User Impact Rating:** 3 (High)
- **Justification:** The crash completely terminates the viewer on macOS during common voice-related operations. With 174 occurrences, it affects a notable number of users. Voice chat is a core social feature, and device switching is a normal user action. However, it appears to require specific timing/race conditions, so not all device switches crash.

**Business Impact Rating:** 2 (Medium)
- **Justification:** This crash affects macOS users specifically, which represents a smaller portion of the user base than Windows. The feature (voice chat) is important but not essential to all users. The crash is in third-party library code (WebRTC), limiting direct fix options.

**Security Impact:** N/A
- This crash is not exploitable. It is an internal assertion failure in the WebRTC library due to threading state inconsistency. No user data is exposed, and the crash cannot be triggered remotely.

**Reproduction Steps:**

1. **Environment Setup:**
   - Use macOS (any recent version)
   - Have multiple audio devices available (built-in speakers, USB headset, Bluetooth headphones)
   - Ensure voice chat is enabled and working
2. Log into Second Life and join a region with voice enabled
3. Enter voice chat (ensure your avatar shows as speaking icon when you speak)
4. While voice is active, rapidly switch audio devices:
   - Open System Preferences → Sound → Output, switch between devices
   - OR plug/unplug a USB headset
   - OR connect/disconnect Bluetooth audio
5. **Expected:** Crash may occur during device switching
6. **Determinism:** This crash is **intermittent** - it depends on race conditions between the audio thread and device switching logic. Rapid successive changes increase crash likelihood.

**QA Pre-Patch Verification:**
1. Set up macOS test machine with multiple audio devices
2. Enter voice chat in Second Life
3. Rapidly switch audio devices while speaking
4. **Verify:** Crash occurs with `libllwebrtc.dylib` and `AudioDeviceMac::StopPlayout` in stack

**Suggested Fix:**

Since the crash is in WebRTC library code, viewer-side fixes focus on safer device management:

1. **Debounce device switching:** Add a cooldown period between device switch operations to prevent rapid successive calls to `workerDeployDevices()`.

2. **Guard against concurrent operations:** Use a mutex or atomic flag to prevent `StopPlayout()` from being called if a previous operation is still in progress.

3. **Graceful error handling:** Wrap the `StopPlayout()` call in a try-catch or check the device module state before calling.

4. **Update WebRTC library:** Check if a newer version of the WebRTC library has fixed this race condition in `AudioDeviceMac`.

**Proposed Implementation:**

```cpp
// In indra/llwebrtc/llwebrtc.cpp, modify workerDeployDevices():

void LLWebRTCImpl::workerDeployDevices()
{
    if (!mDeviceModule)
    {
        return;
    }
    
    // Add guard against rapid successive calls
    static std::chrono::steady_clock::time_point last_deploy_time;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_deploy_time);
    
    // Debounce: minimum 100ms between device deployments
    if (elapsed.count() < 100)
    {
        RTC_LOG(LS_WARNING) << "Skipping rapid device deployment, only " << elapsed.count() << "ms since last";
        // Re-queue if there are pending deployments
        if (mDevicesDeploying.load(std::memory_order_relaxed) > 0)
        {
            mWorkerThread->PostDelayedTask(
                [this] { workerDeployDevices(); },
                webrtc::TimeDelta::Millis(100 - elapsed.count()));
        }
        return;
    }
    last_deploy_time = now;
    
    // Check if device module is in a valid state before stopping
    if (mDeviceModule->Playing())
    {
        mDeviceModule->StopPlayout();
    }
    
    if (mDeviceModule->Recording())
    {
        mDeviceModule->ForceStopRecording();
    }
    
    // ... rest of existing code ...
}
```

**Alternative Recommendation:**

The most robust fix would be to update the WebRTC library to a version that handles this race condition internally. The WebRTC project has active development and may have addressed this in newer releases. Check for:
- WebRTC commits related to `AudioDeviceMac::StopPlayout()` thread safety
- Known issues in WebRTC issue tracker for macOS audio device switching

**Fix Confidence:** 2 (Low-Medium)
- **Justification:** The suggested debouncing fix addresses a symptom (rapid device switching) but not the root cause (race condition in WebRTC's thread management). The actual bug is in the WebRTC library code which we don't directly control. A complete fix requires either WebRTC library updates or deeper integration changes. The debounce approach should reduce crash frequency but may not eliminate it entirely.

**QA Verification - Post-Patch:**
1. Repeat pre-patch reproduction steps on macOS
2. **Verify:** Rapid device switching no longer causes immediate crashes
3. **Verify:** Log messages appear indicating skipped rapid deployments
4. **Verify:** Device switching still works correctly when done at normal human pace
5. **Verify:** Voice chat continues to function after device switches
6. Test edge cases: viewer shutdown during voice, system sleep/wake cycles

