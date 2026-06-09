/**
 * @file llrendertarget.h
 * @brief Off screen render target abstraction.  Loose wrapper for GL_EXT_framebuffer_objects.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLRENDERTARGET_H
#define LL_LLRENDERTARGET_H

// LLRenderTarget is unavailible on the mapserver since it uses FBOs.

#include "llgl.h"
#include "llrender.h"
#include "llgpuresource.h"

/*
 Wrapper around OpenGL framebuffer objects for use in render-to-texture

 SAMPLE USAGE:

    LLRenderTarget target;

    ...

    //allocate a 256x256 RGBA render target with depth buffer
    target.allocate(256,256,GL_RGBA,TRUE);

    //render to contents of offscreen buffer
    target.bindTarget();
    target.clear();
    ... <issue drawing commands> ...
    target.flush();

    ...

    //use target as a texture
    gGL.getTexUnit(INDEX)->bind(&target);
    ... <issue drawing commands> ...

*/

// Inherits LLGPUResource: every public mutator takes an internal UNIQUE lease,
// every self-contained reader takes an internal SHARED lease. Snapshot-return
// accessors (getTexture, getFBO) -- see their docs.
class LLRenderTarget : public LLGPUResource
{
public:
    // Whether or not to use FBO implementation
    static bool sUseFBO;
    static U32 sBytesAllocated;
    static U32 sCurFBO;
    static U32 sCurResX;
    static U32 sCurResY;

    // When true, flush() requires a parent on the RT stack -- i.e. the viewer
    // has reached steady-state rendering with the swap chain attached. When
    // false (early init before swap chain attach, or after shutdown release),
    // flush() falls back to the OS default framebuffer so pre-attach code
    // paths (feature-manager GPU benchmark, etc.) keep working. Toggled by
    // LLSwapChain attach/release.
    // - Geenz 2026-06-08
    static bool sFlushRequiresParent;


    LLRenderTarget();
    ~LLRenderTarget();

    // Movable so std::vector<LLRenderTarget> can reallocate. The
    // user-declared destructor would otherwise suppress implicit move
    // generation, falling back to copy (deleted via LLGPUResource).
    // Only safe to move when no leases are held; see LLGPUResource doc.
    LLRenderTarget(LLRenderTarget&&) noexcept = default;
    LLRenderTarget(const LLRenderTarget&)            = delete;
    LLRenderTarget& operator=(const LLRenderTarget&) = delete;
    LLRenderTarget& operator=(LLRenderTarget&&)      = delete;

    //allocate resources for rendering
    //must be called before use
    //multiple calls will release previously allocated resources
    // resX - width
    // resY - height
    // color_fmt - GL color format (e.g. GL_RGB)
    // depth - if true, allocate a depth buffer
    // usage - deprecated, should always be TT_TEXTURE
    bool allocate(U32 resx, U32 resy, U32 color_fmt, bool depth = false, LLTexUnit::eTextureType usage = LLTexUnit::TT_TEXTURE, LLTexUnit::eTextureMipGeneration generateMipMaps = LLTexUnit::TMG_NONE);

    // Mark this RT as an LLSwapChain image. The flag changes bindTarget's
    // viewport restore behavior: when an intermediate RT flushes back to a
    // swap-chain image, glViewport is restored to gGLViewport (the world
    // view rect) instead of (0,0,mResX,mResY). HUD/UI rendering and
    // renderFinalize's fullscreen triangle expect that.
    void markAsSwapChainImage(bool yes = true);
    bool isSwapChainImage() const;

    // Bind this RT's FBO as GL_READ_FRAMEBUFFER (preserves the current draw
    // FBO). Pair with unbindRead() to restore the read FB to the current
    // draw target. Used by readback consumers (scene monitor) to copy from
    // a swap chain image without ever naming FBO 0 themselves.
    void bindForRead();

    // Restore GL_READ_FRAMEBUFFER to the current draw FBO (sCurFBO). Pairs
    // with bindForRead(). Doesn't read instance state; the symmetric API
    // just keeps the call sites tidy.
    void unbindRead();

    //resize existing attachments to use new resolution and color format
    // CAUTION: if the GL runs out of memory attempting to resize, this render target will be undefined
    // DO NOT use for screen space buffers or for scratch space for an image that might be uploaded
    // DO use for render targets that resize often and aren't likely to ruin someone's day if they break
    void resize(U32 resx, U32 resy);

    //point this render target at a particular LLImageGL
    //   Intended usage:
    //      LLRenderTarget target;
    //      target.addColorAttachment(image);
    //      target.bindTarget();
    //      < issue GL calls>
    //      target.flush();
    //      target.releaseColorAttachment();
    //
    // attachment -- LLImageGL to render into
    // use_name -- optional texture name to target instead of attachment->getTexName()
    // NOTE: setColorAttachment and releaseColorAttachment cannot be used in conjuction with
    // addColorAttachment, allocateDepth, resize, etc.
    void setColorAttachment(LLImageGL* attachment, LLGLuint use_name = 0);

    // detach from current color attachment
    void releaseColorAttachment();

    //add color buffer attachment
    //limit of 4 color attachments per render target
    bool addColorAttachment(U32 color_fmt);

    //allocate a depth texture
    bool allocateDepth();

    //share depth buffer with provided render target
    void shareDepthBuffer(LLRenderTarget& target);

    //free any allocated resources
    //safe to call redundantly
    // asserts that this target is not currently bound or present in the RT stack
    void release();

    //bind target for rendering
    //applies appropriate viewport
    //  If an LLRenderTarget is currently bound, stores a reference to that LLRenderTarget
    //  and restores previous binding on flush() (maintains a stack of Render Targets)
    //  Asserts that this target is not currently bound in the stack
    void bindTarget();

    //clear render targer, clears depth buffer if present,
    //uses scissor rect if in copy-to-texture mode
    // asserts that this target is currently bound
    void clear(U32 mask = 0xFFFFFFFF);

    //get applied viewport
    void getViewport(S32* viewport);

    //get X resolution
    U32 getWidth() const;

    //get Y resolution
    U32 getHeight() const;

    LLTexUnit::eTextureType getUsage() const;

    // Returns a guard that owns a shared lease for the caller's scope. See
    // LLScopedTexName for the safe / unsafe usage patterns.
    LLScopedTexName getTexture(U32 attachment = 0) const;
    U32 getNumTextures() const;

    U32 getDepth() const;

    // Underlying GL framebuffer object name. Exposed for LLSwapChain's
    // present-time blit; viewer code should bind through bindTarget /
    // bindForRead instead of touching this directly. Snapshot-return --
    // caller is responsible for holding a SHARED lease on this RT for the
    // value's use scope.
    U32 getFBO() const;

    void bindTexture(U32 index, S32 channel, LLTexUnit::eTextureFilterOptions filter_options = LLTexUnit::TFO_BILINEAR);

    //flush rendering operations
    //must be called when rendering is complete
    //should be used 1:1 with bindTarget
    // call bindTarget once, do all your rendering, call flush once
    // If an LLRenderTarget was bound when bindTarget was called, binds that RenderTarget for rendering (maintains RT stack)
    // asserts  that this target is currently bound
    void flush();

    //Returns TRUE if target is ready to be rendered into.
    //That is, if the target has been allocated with at least
    //one renderable attachment (i.e. color buffer, depth buffer).
    bool isComplete() const;

    // Returns true if this RenderTarget is bound somewhere in the stack
    bool isBoundInStack() const;

    static LLRenderTarget* getCurrentBoundTarget() { return sBoundTarget; }

    // *HACK
    void swapFBORefs(LLRenderTarget& other);

    static LLRenderTarget* sBoundTarget;

protected:
    U32 mResX;
    U32 mResY;
    std::vector<U32> mTex;
    std::vector<U32> mInternalFormat;
    U32 mFBO;
    LLRenderTarget* mPreviousRT = nullptr;

    U32 mDepth;
    bool mUseDepth;
    LLTexUnit::eTextureMipGeneration mGenerateMipMaps;
    U32 mMipLevels;

    LLTexUnit::eTextureType mUsage;

    // True when this RT is one of LLSwapChain's images. Affects only
    // viewport restore semantics in bindTarget (see markAsSwapChainImage).
    //
    // Future cleanup: replace this flag with a per-RT viewport member
    // (mViewportX/Y/W/H) and have LLSwapChain push gGLViewport updates
    // into its images. Generalizes sub-rect viewports for any RT and maps
    // naturally to Vk/XR state. Held off today because gGLViewport changes
    // more often than window resize (per-frame setup3DViewport, UI scale,
    // sidebar) -- syncing it everywhere is intrusive. Worth taking on as
    // part of the broader render-state cleanup for Vulkan port prep.
    bool mIsSwapChainImage = false;

private:
    // Lockless helpers: do not take a lease. Public wrappers above own the
    // lease and call these; internal callers (allocate -> release_locked,
    // addColorAttachment debug -> bindTarget_locked / flush_locked, flush mip
    // gen -> bindTexture_locked) use them to avoid same-thread re-entry on
    // the shared_mutex.
    void release_locked();
    bool addColorAttachment_locked(U32 color_fmt);
    void bindTarget_locked();
    void flush_locked();
    void bindTexture_locked(U32 index, S32 channel, LLTexUnit::eTextureFilterOptions filter_options);
};

#endif

