/**
 * @file llswapchain.cpp
 * @brief LLSwapChain implementation (OpenGL backend).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
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

#include "llswapchain.h"

#include "llgl.h"
#include "llwindow.h"

LLSwapChain::~LLSwapChain()
{
    release();
}

void LLSwapChain::attachToWindow(LLWindow* window, U32 width, U32 height)
{
    llassert(window != nullptr);
    llassert(mImages.empty()); // call release() before re-attaching
    llassert(width > 0 && height > 0);

    mWindow = window;
    mWidth  = width;
    mHeight = height;

    // Allocate kImageCount off-screen color+depth RTs at window resolution.
    // Each carries the swap-chain-image flag so its bindTarget restores the
    // world-view viewport when intermediate RTs pop back to it during rendering.
    mImages.resize(kImageCount);
    for (auto& img : mImages)
    {
        if (!img.allocate(width, height, GL_RGBA, /*depth=*/true))
        {
            LL_WARNS("SwapChain") << "Failed to allocate swap chain image at "
                                  << width << "x" << height << LL_ENDL;
        }
        img.markAsSwapChainImage(true);
    }

    mCurrentIndex = 0;

    // From now on, top-level RT flushes must have a parent on the stack.
    // Pre-attach paths (gpu_benchmark in feature-manager init, etc.) ran
    // before this point and used the FBO-0 fallback inside flush().
    LLRenderTarget::sFlushRequiresParent = true;
}

void LLSwapChain::resize(U32 width, U32 height)
{
    if (width == 0 || height == 0)
    {
        return; // minimized / iconified -- keep existing storage
    }

    mWidth  = width;
    mHeight = height;

    for (auto& img : mImages)
    {
        img.resize(width, height);
    }
}

LLRenderTarget& LLSwapChain::acquireNextImage()
{
    llassert(!mImages.empty());

    // Rotate to the next image. GL has no real "acquire" -- the driver owns
    // the back buffer rotation under SwapBuffers -- so this is just structural
    // cycling. Vk/XR backends will do the real WSI acquire here.
    mCurrentIndex = (mCurrentIndex + 1) % (U32)mImages.size();
    return mImages[mCurrentIndex];
}

LLRenderTarget& LLSwapChain::getCurrentImage()
{
    llassert(!mImages.empty());
    return mImages[mCurrentIndex];
}

LLRenderTarget& LLSwapChain::getPreviousImage()
{
    llassert(!mImages.empty());
    const U32 n = (U32)mImages.size();
    const U32 prev = (mCurrentIndex + n - 1) % n;
    return mImages[prev];
}

void LLSwapChain::present()
{
    llassert(!mImages.empty());
    llassert(mWindow != nullptr);

    LLRenderTarget& img = mImages[mCurrentIndex];

    // Blit the current image's color into FBO 0, then SwapBuffers.
    // This is the only place in the codebase that names FBO 0 by literal.
    {
        LL_PROFILE_GPU_ZONE("swapchain present blit");

        // Hold a shared lease on the image across the blit -- getFBO is
        // snapshot-return, the value must stay valid for the call.
        LLSharedLease img_lease = img.getSharedLease();

        // Save current read FB so we don't disturb anyone else's state.
        // (sCurFBO tracks the current draw FB; flush() asserts it matches.)
        const U32 prev_fbo = LLRenderTarget::sCurFBO;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, img.getFBO());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);

        glBlitFramebuffer(0, 0, (GLint)mWidth, (GLint)mHeight,
                          0, 0, (GLint)mWidth, (GLint)mHeight,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Restore unified read+draw binding to whatever was current before.
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    }

    mWindow->swapBuffers();
}

void LLSwapChain::release()
{
    // Drop the strict-flush requirement so any late-shutdown top-level
    // flushes don't trip the assert.
    LLRenderTarget::sFlushRequiresParent = false;

    // LLRenderTarget destructors release GL resources.
    mImages.clear();
    mWindow       = nullptr;
    mWidth        = 0;
    mHeight       = 0;
    mCurrentIndex = 0;
}
