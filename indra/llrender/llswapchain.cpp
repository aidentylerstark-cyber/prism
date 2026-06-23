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
#include "llrendertarget.h"
#include "llwindow.h"


LLSwapChain::~LLSwapChain()
{
    release();
}

void LLSwapChain::attachToWindow(LLWindow* window, U32 width, U32 height)
{
    llassert(window != nullptr);
    llassert(width > 0 && height > 0);

    mWindow = window;
    mWidth  = width;
    mHeight = height;

    // From now on, top-level RT flushes must have a parent on the stack.
    // Pre-attach paths (gpu_benchmark in feature-manager init, etc.) ran
    // before this point and used the FBO-0 fallback inside flush().
    LLRenderTarget::sFlushRequiresParent = true;
}

void LLSwapChain::resize(U32 width, U32 height)
{
    if (width == 0 || height == 0)
    {
        return; // minimized / iconified - keep existing dimensions
    }

    mWidth  = width;
    mHeight = height;
    // GL: the default framebuffer follows the window; nothing to reallocate.
}

void LLSwapChain::acquireNextImage()
{
    // GL: no real acquire - the driver owns the back-buffer rotation under
    // SwapBuffers. Vk/XR backends do the WSI acquire here.
}

void LLSwapChain::bindForRender()
{
    // Composite straight into the window's default framebuffer (FBO 0), full
    // window - zero-copy. The driver double-buffers under swapBuffers. FBO 0 is
    // named only here; LLRenderTarget never knows about it.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    LLRenderTarget::sCurFBO = 0;
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glViewport(0, 0, mWidth, mHeight);
    LLRenderTarget::sCurResX = mWidth;
    LLRenderTarget::sCurResY = mHeight;
}

void LLSwapChain::present()
{
    llassert(mWindow != nullptr);

    // The layers were composited straight into the back buffer (FBO 0), so
    // just swap. Vk/XR backends issue the real present here.
    mWindow->swapBuffers();
}

void LLSwapChain::bindForRead()
{
    // Read the default framebuffer (the on-screen frame) for readback.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
}

void LLSwapChain::unbindRead()
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, LLRenderTarget::sCurFBO);
    glReadBuffer(LLRenderTarget::sCurFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK);
}

void LLSwapChain::release()
{
    // Drop the strict-flush requirement so any late-shutdown top-level
    // flushes don't trip the assert.
    LLRenderTarget::sFlushRequiresParent = false;

    mWindow = nullptr;
    mWidth  = 0;
    mHeight = 0;
}
