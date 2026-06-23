/**
 * @file llswapchain.h
 * @brief Backend-agnostic presentation surface for the window.
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

#ifndef LL_LLSWAPCHAIN_H
#define LL_LLSWAPCHAIN_H

#include "llrendertarget.h"

class LLWindow;

// Backend-agnostic presentation surface. The compositor acquires the surface
// each frame, binds it as the render target, composites into it, then presents.
//
// GL backend (today): zero-copy, and there are no images to own. The present
// surface is the window's default framebuffer (FBO 0): bindForRender() binds 0
// with the full-window viewport, present() calls swapBuffers(). The driver
// double-buffers the back buffer under SwapBuffers, so the swap chain just
// holds the window and dimensions. FBO 0 is named only here - never in
// LLRenderTarget, which stays a pure off-screen RT.
//
// Vulkan / OpenXR backends (future): own the runtime's real swap-chain images.
// acquireNextImage becomes vkAcquireNextImageKHR / xrAcquireSwapchainImage,
// bindForRender binds the acquired image's framebuffer, and present becomes
// vkQueuePresentKHR / xrReleaseSwapchainImage. The compositor interface is
// unchanged.
class LLSwapChain
{
public:
    LLSwapChain() = default;
    ~LLSwapChain();

    LLSwapChain(const LLSwapChain&) = delete;
    LLSwapChain& operator=(const LLSwapChain&) = delete;

    // GL: remember the window so present() can swapBuffers; record dimensions.
    void attachToWindow(LLWindow* window, U32 width, U32 height);

    // Update the presentation dimensions. GL: FBO 0 follows the window, so this
    // just records the size - nothing to reallocate. No-op on zero dimensions.
    void resize(U32 width, U32 height);

    // Acquire the next presentable image. GL: structural - the driver owns the
    // back-buffer rotation under SwapBuffers. Vk/XR: the real WSI acquire.
    // Balance 1:1 with present().
    void acquireNextImage();

    // Bind the acquired image as the render target for compositing. GL: binds
    // the default framebuffer (FBO 0) with the full-window viewport.
    void bindForRender();

    // Present the composited image. GL: swapBuffers(). Vk/XR: the real present.
    void present();

    // Bind the present surface as GL_READ_FRAMEBUFFER for readback consumers
    // (the scene monitor reading the on-screen frame). Pair with unbindRead().
    void bindForRead();
    void unbindRead();

    // Drop resources and detach. Safe to call redundantly.
    void release();

    // True once attachToWindow has run (and before release).
    bool isAttached() const { return mWindow != nullptr; }

    U32 getWidth() const  { return mWidth; }
    U32 getHeight() const { return mHeight; }

private:
    LLWindow*  mWindow = nullptr;
    U32        mWidth  = 0;
    U32        mHeight = 0;
};

#endif // LL_LLSWAPCHAIN_H
