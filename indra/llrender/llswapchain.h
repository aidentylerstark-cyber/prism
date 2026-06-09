/**
 * @file llswapchain.h
 * @brief Backend-agnostic swap chain wrapping the window's presentation images.
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

#include <vector>

class LLWindow;

// Owns the collection of LLRenderTargets that act as the window's presentation
// images, plus the present primitive itself. The viewer acquires an image
// each frame, renders into it via the usual LLRenderTarget interface, then
// asks the swap chain to present.
//
// GL backend (today): the chain holds N off-screen color+depth FBOs sized to
// the window. acquireNextImage() rotates the index. present() blits the
// current image's color to FBO 0 and calls swapBuffers(). The literal 0
// only appears inside present() — viewer code never names it.
//
// Vulkan / OpenXR backends (future): mImages map onto the runtime's actual
// swap chain images; acquireNextImage / present become the real WSI calls
// (vkAcquireNextImageKHR / vkQueuePresentKHR or xrAcquire/Release pairs).
// The viewer code that uses this interface doesn't change.
class LLSwapChain
{
public:
    // How many images cycle in the chain. 2 is the minimum useful number;
    // there is no parallelism gain on GL above 2 (the driver does its own
    // back-buffer rotation under SwapBuffers).
    static constexpr U32 kImageCount = 2;

    LLSwapChain() = default;
    ~LLSwapChain();

    LLSwapChain(const LLSwapChain&) = delete;
    LLSwapChain& operator=(const LLSwapChain&) = delete;

    // GL backend: allocates kImageCount off-screen color+depth RTs at
    // (width, height) and remembers the window so present() can swapBuffers.
    void attachToWindow(LLWindow* window, U32 width, U32 height);

    // Resize all images to (width, height). No-op on zero dimensions.
    void resize(U32 width, U32 height);

    // Rotate to the next image in the chain and return it. Render into the
    // returned RT this frame; call present() when done. Each call to
    // acquireNextImage() should be balanced 1:1 with a present().
    LLRenderTarget& acquireNextImage();

    // Image currently being rendered into (or just rendered). Use for
    // readback consumers that want the in-progress frame.
    LLRenderTarget& getCurrentImage();

    // Most recently presented image — i.e. the one whose contents are on
    // screen right now. Useful for consumers (scene monitor) that want the
    // last visible frame rather than the half-rendered current one.
    LLRenderTarget& getPreviousImage();

    // Blit the current image's color to FBO 0 and call window->swapBuffers().
    void present();

    // Drop image resources and detach. Safe to call redundantly.
    void release();

    U32 getWidth() const  { return mWidth; }
    U32 getHeight() const { return mHeight; }
    U32 getImageCount() const { return (U32)mImages.size(); }

private:
    std::vector<LLRenderTarget> mImages;
    U32        mCurrentIndex = 0;
    LLWindow*  mWindow       = nullptr;
    U32        mWidth        = 0;
    U32        mHeight       = 0;
};

#endif // LL_LLSWAPCHAIN_H
