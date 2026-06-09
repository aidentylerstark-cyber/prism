/**
 * @file llrendertarget.cpp
 * @brief LLRenderTarget implementation
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

#include "linden_common.h"

#include "llrendertarget.h"
#include "llrender.h"
#include "llgl.h"

thread_local LLRenderTarget* LLRenderTarget::sBoundTarget = NULL;
U32 LLRenderTarget::sBytesAllocated = 0;

void check_framebuffer_status()
{
    if (gDebugGL)
    {
        GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        switch (status)
        {
        case GL_FRAMEBUFFER_COMPLETE:
            break;
        default:
            LL_WARNS() << "check_framebuffer_status failed -- " << std::hex << status << LL_ENDL;
            ll_fail("check_framebuffer_status failed");
            break;
        }
    }
}

bool LLRenderTarget::sUseFBO = false;
thread_local U32 LLRenderTarget::sCurFBO = 0;
thread_local bool LLRenderTarget::sFlushRequiresParent = false;


extern S32 gGLViewport[4];

thread_local U32 LLRenderTarget::sCurResX = 0;
thread_local U32 LLRenderTarget::sCurResY = 0;

LLRenderTarget::LLRenderTarget() :
    mResX(0),
    mResY(0),
    mFBO(0),
    mDepth(0),
    mUseDepth(false),
    mUsage(LLTexUnit::TT_TEXTURE)
{
}

LLRenderTarget::~LLRenderTarget()
{
    release();
}

// ---------------------------------------------------------------------------
// Public mutators - each takes a unique lease and delegates to a _locked
// helper if it needs to call into another mutator.
// ---------------------------------------------------------------------------

void LLRenderTarget::resize(U32 resx, U32 resy)
{
    LLUniqueLease lease = getUniqueLease();

    S32 pix_diff = (resx*resy)-(mResX*mResY);

    mResX = resx;
    mResY = resy;

    llassert(mInternalFormat.size() == mTex.size());

    for (U32 i = 0; i < mTex.size(); ++i)
    {
        gGL.getTexUnit(0)->bindManual(mUsage, mTex[i]);
        LLImageGL::setManualImage(LLTexUnit::getInternalType(mUsage), 0, mInternalFormat[i], mResX, mResY, GL_RGBA, GL_UNSIGNED_BYTE, NULL, false);
        sBytesAllocated += pix_diff*4;
    }

    if (mDepth)
    {
        gGL.getTexUnit(0)->bindManual(mUsage, mDepth);
        U32 internal_type = LLTexUnit::getInternalType(mUsage);
        LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24, mResX, mResY, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL, false);

        sBytesAllocated += pix_diff*4;
    }
}

bool LLRenderTarget::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth, LLTexUnit::eTextureType usage, LLTexUnit::eTextureMipGeneration generateMipMaps)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(usage == LLTexUnit::TT_TEXTURE);
    llassert(!isBoundInStack());

    LLUniqueLease lease = getUniqueLease();

    resx = llmin(resx, (U32) gGLManager.mGLMaxTextureSize);
    resy = llmin(resy, (U32) gGLManager.mGLMaxTextureSize);

    release_locked();

    mResX = resx;
    mResY = resy;

    mUsage = usage;
    mUseDepth = depth;

    mGenerateMipMaps = generateMipMaps;

    if (mGenerateMipMaps != LLTexUnit::TMG_NONE) {
        mMipLevels = 1 + (U32)floor(log10((float)llmax(mResX, mResY)) / log10(2.0));
    }

    if (depth)
    {
        // Inlined allocateDepth body - still under our unique lease.
        LLImageGL::generateTextures(1, &mDepth);
        gGL.getTexUnit(0)->bindManual(mUsage, mDepth);
        U32 internal_type = LLTexUnit::getInternalType(mUsage);
        stop_glerror();
        clear_glerror();
        LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24, mResX, mResY, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL, false);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
        sBytesAllocated += mResX*mResY*4;
        if (glGetError() != GL_NO_ERROR)
        {
            LL_WARNS() << "Unable to allocate depth buffer for render target." << LL_ENDL;
            return false;
        }
    }

    glGenFramebuffers(1, (GLuint *) &mFBO);

    if (mDepth)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), mDepth, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
    }

    return addColorAttachment_locked(color_fmt);
}

void LLRenderTarget::setColorAttachment(LLImageGL* img, LLGLuint use_name)
{
    LL_PROFILE_ZONE_SCOPED;
    llassert(img != nullptr);
    llassert(sUseFBO);
    llassert(mDepth == 0);
    llassert(mTex.empty());
    llassert(!isBoundInStack());

    LLUniqueLease lease = getUniqueLease();

    if (mFBO == 0)
    {
        glGenFramebuffers(1, (GLuint*)&mFBO);
    }

    mResX = img->getWidth();
    mResY = img->getHeight();
    mUsage = img->getTarget();

    // Guarded read on img (different LLGPUResource, different mutex - safe).
    LLScopedTexName guard;
    if (use_name == 0)
    {
        guard = img->getTexName();
        use_name = guard.get();
    }

    mTex.push_back(use_name);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            LLTexUnit::getInternalType(mUsage), use_name, 0);
        stop_glerror();

    check_framebuffer_status();

    glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
}

void LLRenderTarget::releaseColorAttachment()
{
    LL_PROFILE_ZONE_SCOPED;
    llassert(!isBoundInStack());
    llassert(mTex.size() == 1);
    llassert(mFBO != 0);

    LLUniqueLease lease = getUniqueLease();

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, LLTexUnit::getInternalType(mUsage), 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);

    mTex.clear();
}

bool LLRenderTarget::addColorAttachment(U32 color_fmt)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());

    LLUniqueLease lease = getUniqueLease();
    return addColorAttachment_locked(color_fmt);
}

bool LLRenderTarget::addColorAttachment_locked(U32 color_fmt)
{
    if (color_fmt == 0)
    {
        return true;
    }

    U32 offset = static_cast<U32>(mTex.size());

    if( offset >= 4 )
    {
        LL_WARNS() << "Too many color attachments" << LL_ENDL;
        llassert( offset < 4 );
        return false;
    }
    if( offset > 0 && (mFBO == 0) )
    {
        llassert(  mFBO != 0 );
        return false;
    }

    U32 tex;
    LLImageGL::generateTextures(1, &tex);
    gGL.getTexUnit(0)->bindManual(mUsage, tex);

    stop_glerror();

    {
        clear_glerror();
        LLImageGL::setManualImage(LLTexUnit::getInternalType(mUsage), 0, color_fmt, mResX, mResY, GL_RGBA, GL_UNSIGNED_BYTE, NULL, false);
        if (glGetError() != GL_NO_ERROR)
        {
            LL_WARNS() << "Could not allocate color buffer for render target." << LL_ENDL;
            return false;
        }
    }

    sBytesAllocated += mResX*mResY*4;

    stop_glerror();

    if (offset == 0)
    {
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
        stop_glerror();
    }
    else
    {
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
        stop_glerror();
    }

    if (mUsage != LLTexUnit::TT_RECT_TEXTURE)
    {
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_MIRROR);
        stop_glerror();
    }
    else
    {
        // ATI doesn't support mirrored repeat for rectangular textures.
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
        stop_glerror();
    }

    if (mFBO)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+offset,
            LLTexUnit::getInternalType(mUsage), tex, 0);

        check_framebuffer_status();

        glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
    }

    mTex.push_back(tex);
    mInternalFormat.push_back(color_fmt);

    if (gDebugGL)
    { //bind and unbind to validate target
        bindTarget_locked();
        flush_locked();
    }

    return true;
}

bool LLRenderTarget::allocateDepth()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LLUniqueLease lease = getUniqueLease();

    LLImageGL::generateTextures(1, &mDepth);
    gGL.getTexUnit(0)->bindManual(mUsage, mDepth);

    U32 internal_type = LLTexUnit::getInternalType(mUsage);
    stop_glerror();
    clear_glerror();
    LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24, mResX, mResY, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL, false);
    gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);

    sBytesAllocated += mResX*mResY*4;

    if (glGetError() != GL_NO_ERROR)
    {
        LL_WARNS() << "Unable to allocate depth buffer for render target." << LL_ENDL;
        return false;
    }

    return true;
}

void LLRenderTarget::shareDepthBuffer(LLRenderTarget& target)
{
    llassert(!isBoundInStack());

    // Lock this first, then target - keep the order consistent so we
    // can't deadlock.
    LLUniqueLease lease_this = getUniqueLease();
    LLUniqueLease lease_other = target.getUniqueLease();

    if (!mFBO || !target.mFBO)
    {
        LL_ERRS() << "Cannot share depth buffer between non FBO render targets." << LL_ENDL;
    }

    if (target.mDepth)
    {
        LL_ERRS() << "Attempting to override existing depth buffer.  Detach existing buffer first." << LL_ENDL;
    }

    if (target.mUseDepth)
    {
        LL_ERRS() << "Attempting to override existing shared depth buffer. Detach existing buffer first." << LL_ENDL;
    }

    if (mDepth)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target.mFBO);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), mDepth, 0);

        check_framebuffer_status();

        glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);

        target.mUseDepth = true;
    }
}

void LLRenderTarget::release()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());

    LLUniqueLease lease = getUniqueLease();
    release_locked();
}

namespace
{
    // Wrap a fresh fence in a shared_ptr that deletes it when the last
    // reference drops. Sync objects belong to the share group, so either
    // context can do the delete.
    std::shared_ptr<void> make_fence()
    {
        return std::shared_ptr<void>(
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0),
            [](void* sync) { if (sync) glDeleteSync((GLsync)sync); });
    }
}

void LLRenderTarget::placeFrameCompleteFence()
{
    if (!mCrossSync) return;
    std::shared_ptr<void> fence = make_fence();
    std::lock_guard<std::mutex> lock(mCrossSync->fenceMutex);
    mCrossSync->frameCompleteFence = std::move(fence);
}

void LLRenderTarget::waitFrameCompleteFence()
{
    if (!mCrossSync) return;
    std::shared_ptr<void> fence;
    {
        std::lock_guard<std::mutex> lock(mCrossSync->fenceMutex);
        fence = mCrossSync->frameCompleteFence;
    }
    if (fence)
    {
        // Server-side wait so we don't stall the CPU. Holding the
        // shared_ptr keeps the sync alive even if the producer swaps in
        // a new one mid-wait.
        glWaitSync((GLsync)fence.get(), 0, GL_TIMEOUT_IGNORED);
    }
}

void LLRenderTarget::placeReadCompleteFence()
{
    if (!mCrossSync) return;
    std::shared_ptr<void> fence = make_fence();
    // Flush so the other context can actually see the fence.
    glFlush();
    std::lock_guard<std::mutex> lock(mCrossSync->fenceMutex);
    mCrossSync->readCompleteFence = std::move(fence);
}

void LLRenderTarget::waitReadCompleteFence()
{
    if (!mCrossSync) return;
    std::shared_ptr<void> fence;
    {
        std::lock_guard<std::mutex> lock(mCrossSync->fenceMutex);
        fence = mCrossSync->readCompleteFence;
    }
    if (fence)
    {
        glWaitSync((GLsync)fence.get(), 0, GL_TIMEOUT_IGNORED);
    }
}

void LLRenderTarget::release_locked()
{
    mIsSwapChainImage = false;

    // Drops the fences; glDeleteSync runs when the last holder lets go.
    mCrossSync.reset();

    if (mDepth)
    {
        LLImageGL::deleteTextures(1, &mDepth);

        mDepth = 0;

        sBytesAllocated -= mResX*mResY*4;
    }
    else if (mFBO)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

        if (mUseDepth)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), 0, 0);
            mUseDepth = false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
    }

    if (mFBO && (mTex.size() > 1))
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        size_t z;
        for (z = mTex.size() - 1; z >= 1; z--)
        {
            sBytesAllocated -= mResX*mResY*4;
            glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0+z), LLTexUnit::getInternalType(mUsage), 0, 0);
            LLImageGL::deleteTextures(1, &mTex[z]);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
    }

    if (mFBO)
    {
        if (mFBO == sCurFBO)
        {
            sCurFBO = 0;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glDeleteFramebuffers(1, (GLuint *) &mFBO);
        mFBO = 0;
    }

    if (mTex.size() > 0)
    {
        sBytesAllocated -= mResX*mResY*4;
        LLImageGL::deleteTextures(1, &mTex[0]);
    }

    mTex.clear();
    mInternalFormat.clear();

    mResX = mResY = 0;
}

void LLRenderTarget::bindTarget()
{
    LL_PROFILE_GPU_ZONE("bindTarget");
    llassert(!isBoundInStack());
    llassert(mFBO);

    LLUniqueLease lease = getUniqueLease();
    bindTarget_locked();
}

void LLRenderTarget::bindTarget_locked()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    sCurFBO = mFBO;

    GLenum drawbuffers[] = {GL_COLOR_ATTACHMENT0,
                            GL_COLOR_ATTACHMENT1,
                            GL_COLOR_ATTACHMENT2,
                            GL_COLOR_ATTACHMENT3};

    if (mTex.empty())
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        glDrawBuffers(static_cast<GLsizei>(mTex.size()), drawbuffers);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }
    check_framebuffer_status();

    if (mIsSwapChainImage)
    {
        // Bottom-of-stack swap chain image: restore the world-view viewport
        // (gGLViewport) so HUD/UI and renderFinalize's fullscreen triangle
        // render into the same sub-rect they did when we drew straight to
        // FBO 0. The off-screen image is window-sized; the viewport is the
        // world-view rect inside it.
        // - Geenz 2026-06-08
        glViewport(gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]);
        sCurResX = gGLViewport[2];
        sCurResY = gGLViewport[3];
    }
    else
    {
        glViewport(0, 0, mResX, mResY);
        sCurResX = mResX;
        sCurResY = mResY;
    }

    mPreviousRT = sBoundTarget;
    sBoundTarget = this;
}

void LLRenderTarget::clear(U32 mask_in)
{
    LL_PROFILE_GPU_ZONE("clear");
    llassert(mFBO);

    LLUniqueLease lease = getUniqueLease();

    U32 mask = GL_COLOR_BUFFER_BIT;
    if (mUseDepth)
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (mFBO)
    {
        check_framebuffer_status();
        stop_glerror();
        glClear(mask & mask_in);
        stop_glerror();
    }
    else
    {
        LLGLEnable scissor(GL_SCISSOR_TEST);
        glScissor(0, 0, mResX, mResY);
        stop_glerror();
        glClear(mask & mask_in);
    }
}

LLScopedTexName LLRenderTarget::getTexture(U32 attachment) const
{
    LLSharedLease lease = getSharedLease();
    if (attachment >= mTex.size())
    {
        LL_WARNS() << "Invalid attachment index " << attachment << " for size " << mTex.size() << LL_ENDL;
        llassert(false);
        return LLScopedTexName(std::move(lease), 0);
    }
    LLGLuint name = mTex[attachment];
    return LLScopedTexName(std::move(lease), name);
}

U32 LLRenderTarget::getNumTextures() const
{
    LLSharedLease lease = getSharedLease();
    return static_cast<U32>(mTex.size());
}

void LLRenderTarget::bindTexture(U32 index, S32 channel, LLTexUnit::eTextureFilterOptions filter_options)
{
    LLSharedLease lease = getSharedLease();
    bindTexture_locked(index, channel, filter_options);
}

void LLRenderTarget::bindTexture_locked(U32 index, S32 channel, LLTexUnit::eTextureFilterOptions filter_options)
{
    // Read mTex directly - calling getTexture() here would re-enter the
    // lease we already hold.
    LLGLuint name = (index < mTex.size()) ? mTex[index] : 0;
    gGL.getTexUnit(channel)->bindManual(mUsage, name, filter_options == LLTexUnit::TFO_TRILINEAR || filter_options == LLTexUnit::TFO_ANISOTROPIC);
    gGL.getTexUnit(channel)->setTextureFilteringOption(filter_options);
}

void LLRenderTarget::flush()
{
    LL_PROFILE_GPU_ZONE("rt flush");
    gGL.flush();
    llassert(sBoundTarget == this);
    llassert(mFBO);
    llassert(sCurFBO == mFBO);

    LLUniqueLease lease = getUniqueLease();
    flush_locked();
}

void LLRenderTarget::flush_locked()
{
    if (mGenerateMipMaps == LLTexUnit::TMG_AUTO)
    {
        LL_PROFILE_GPU_ZONE("rt generate mipmaps");
        bindTexture_locked(0, 0, LLTexUnit::TFO_TRILINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    if (mPreviousRT)
    {
        // a bit hacky -- pop the RT stack back two frames and push
        // the previous frame back on to play nice with the GL state machine
        sBoundTarget = mPreviousRT->mPreviousRT;
        mPreviousRT->bindTarget();
        return;
    }

    if (mIsSwapChainImage)
    {
        // Bottom of the stack for a render batch. Just pop the bookkeeping
        // and leave the FBO bound - LLSwapChain::present() will rebind FBO 0
        // for the blit, and the next acquireNextImage() will set up the next
        // image's bind.
        sBoundTarget = nullptr;
        return;
    }

    // No parent on the stack and not a swap chain image. While the swap chain
    // is attached (steady-state rendering) this is a missed bind site -
    // assert so the call site can be wrapped. Before the chain attaches or
    // after release (feature-manager GPU benchmark, late shutdown cleanup)
    // fall back to the OS default framebuffer the way the old code did.
    // - Geenz 2026-06-08
    llassert(!sFlushRequiresParent);

    sBoundTarget = nullptr;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    sCurFBO = 0;
    glViewport(gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]);
    sCurResX = gGLViewport[2];
    sCurResY = gGLViewport[3];
    glReadBuffer(GL_BACK);
    glDrawBuffer(GL_BACK);
}

void LLRenderTarget::bindForRead()
{
    LLSharedLease lease = getSharedLease();
    llassert(mFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
}

void LLRenderTarget::unbindRead()
{
    // Doesn't read instance state - no lease needed.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sCurFBO);
    glReadBuffer(sCurFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK);
}

bool LLRenderTarget::isComplete() const
{
    LLSharedLease lease = getSharedLease();
    return !mTex.empty() || mDepth;
}

void LLRenderTarget::getViewport(S32* viewport)
{
    LLSharedLease lease = getSharedLease();
    viewport[0] = 0;
    viewport[1] = 0;
    viewport[2] = mResX;
    viewport[3] = mResY;
}

bool LLRenderTarget::isBoundInStack() const
{
    // No lease here - this walks other RTs' fields, and locking them from
    // here just invites deadlock. It's a debug probe; racy reads are fine.
    LLRenderTarget* cur = sBoundTarget;
    while (cur && cur != this)
    {
        cur = cur->mPreviousRT;
    }

    return cur == this;
}

void LLRenderTarget::swapFBORefs(LLRenderTarget& other)
{
    // Two-resource lock: this then other (consistent order).
    LLUniqueLease lease_this = getUniqueLease();
    LLUniqueLease lease_other = other.getUniqueLease();

    llassert(mFBO);
    llassert(other.mFBO);

    llassert(sCurFBO != mFBO);
    llassert(sCurFBO != other.mFBO);
    llassert(!isBoundInStack());
    llassert(!other.isBoundInStack());

    llassert(sUseFBO == other.sUseFBO);
    llassert(mResX == other.mResX);
    llassert(mResY == other.mResY);
    llassert(mInternalFormat == other.mInternalFormat);
    llassert(mTex.size() == other.mTex.size());
    llassert(mDepth == other.mDepth);
    llassert(mUseDepth == other.mUseDepth);
    llassert(mGenerateMipMaps == other.mGenerateMipMaps);
    llassert(mMipLevels == other.mMipLevels);
    llassert(mUsage == other.mUsage);

    std::swap(mFBO, other.mFBO);
    std::swap(mTex, other.mTex);
}

// ---------------------------------------------------------------------------
// Simple scalar readers - each takes its own shared lease.
// ---------------------------------------------------------------------------

U32 LLRenderTarget::getWidth() const
{
    LLSharedLease lease = getSharedLease();
    return mResX;
}

U32 LLRenderTarget::getHeight() const
{
    LLSharedLease lease = getSharedLease();
    return mResY;
}

LLTexUnit::eTextureType LLRenderTarget::getUsage() const
{
    LLSharedLease lease = getSharedLease();
    return mUsage;
}

U32 LLRenderTarget::getDepth() const
{
    LLSharedLease lease = getSharedLease();
    return mDepth;
}

U32 LLRenderTarget::getFBO() const
{
    // Just a snapshot - callers that need the value to stay valid should
    // hold a shared lease themselves.
    return mFBO;
}

void LLRenderTarget::markAsSwapChainImage(bool yes)
{
    LLUniqueLease lease = getUniqueLease();
    mIsSwapChainImage = yes;
}

bool LLRenderTarget::isSwapChainImage() const
{
    LLSharedLease lease = getSharedLease();
    return mIsSwapChainImage;
}
