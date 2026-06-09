/**
 * @file llcompositor.cpp
 * @brief LLCompositor implementation.
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

#include "llcompositor.h"

#include "lltestsquarecompositable.h"
#include "lltimer.h"
#include "llrendertarget.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llwindow.h"

#include <algorithm>

LLCompositor::LLCompositor() = default;

LLCompositor::~LLCompositor()
{
    release();
}

void LLCompositor::attachToWindow(LLWindow* window, U32 width, U32 height)
{
    mWindow = window;
    mSwapChain.attachToWindow(window, width, height);
}

void LLCompositor::createRefreshOverlay()
{
    // Runs on our own thread - the squares each spin up a producer
    // thread with its own shared GL context, which needs our context
    // current (it is, inside presentFrame).
    if (mRefreshShown || !mWindow)
    {
        return;
    }
    mTestSquares.emplace_back(std::make_unique<LLTestSquareCompositable>(
        "Blue Square", 0, 64, 255, 1u, 40, 40, mWindow));
    mTestSquares.emplace_back(std::make_unique<LLTestSquareCompositable>(
        "Orange Square", 255, 128, 0, 2u, 180, 40, mWindow));
    mTestSquares.emplace_back(std::make_unique<LLTestSquareCompositable>(
        "Red Square", 255, 0, 0, 3u, 320, 40, mWindow));
    mTestSquares.emplace_back(std::make_unique<LLTestSquareCompositable>(
        "Purple Square", 160, 0, 255, 4u, 460, 40, mWindow));
    for (auto& square : mTestSquares)
    {
        square->connect(*this);
        addCompositable(square.get());
    }
    mRefreshShown = true;
}

void LLCompositor::destroyRefreshOverlay()
{
    if (!mRefreshShown)
    {
        return;
    }
    // Drop them from the draw list before tearing down so the present
    // loop never touches a destroyed square.
    for (auto& square : mTestSquares)
    {
        removeCompositable(square.get());
        square->disconnect();
    }
    mTestSquares.clear();
    mRefreshShown = false;
}

void LLCompositor::resize(U32 width, U32 height)
{
    mSwapChain.resize(width, height);
}

void LLCompositor::release()
{
    destroyRefreshOverlay();
    mCompositables.clear();
    mBlitShader = nullptr; // owned by LLViewerShaderMgr
    mSwapChain.release();
    // The window is destroyed right after us; drop our pointer and any
    // queued work so a late present/toggle can't touch it.
    mWindow = nullptr;
    mPendingSwapInterval.store(-1, std::memory_order_relaxed);
    mPendingShowRefresh.store(-1, std::memory_order_relaxed);
}

void LLCompositor::addCompositable(LLCompositable* c)
{
    llassert(c != nullptr);
    // The stats overlay iterates this list from the viewer thread.
    std::lock_guard<std::mutex> lock(mCompositablesMutex);
    mCompositables.push_back(c);
}

void LLCompositor::removeCompositable(LLCompositable* c)
{
    std::lock_guard<std::mutex> lock(mCompositablesMutex);
    mCompositables.erase(
        std::remove(mCompositables.begin(), mCompositables.end(), c),
        mCompositables.end());
}

void LLCompositor::getLayerStats(std::vector<LayerStatsSnapshot>& out) const
{
    out.clear();
    std::lock_guard<std::mutex> lock(mCompositablesMutex);
    for (LLCompositable* c : mCompositables)
    {
        const F32 frame_ms = c->lastFrameMs();
        out.push_back({c->compositableName(),
                       frame_ms,
                       frame_ms > 0.f ? 1000.f / frame_ms : 0.f});
    }
}


void LLCompositor::presentFrame()
{
    LL_PROFILE_ZONE_SCOPED;

    // The RT stack should be empty when we get here.
    llassert(LLRenderTarget::getCurrentBoundTarget() == nullptr);

    // Apply a pending swap interval on our own context.
    const S32 pending_interval = mPendingSwapInterval.exchange(-1, std::memory_order_relaxed);
    if (pending_interval >= 0 && mWindow)
    {
        mWindow->setSwapInterval(pending_interval);
    }

    // Apply a pending refresh-overlay toggle here, where our GL context
    // is current for the squares' context creation/teardown.
    const S32 pending_show = mPendingShowRefresh.exchange(-1, std::memory_order_relaxed);
    if (pending_show == 1)
    {
        createRefreshOverlay();
    }
    else if (pending_show == 0)
    {
        destroyRefreshOverlay();
    }

    // Draw every layer's front texture as a quad straight into the default
    // framebuffer, bottom first. Binding the texture for sampling is also
    // what pulls the producer context's writes into this one.
    //
    // The blit shader lives in LLViewerShaderMgr (compositorblitV/F.glsl).
    // Until it's handed to us and compiled, just present - nothing to
    // draw with yet.
    const bool shader_ready = mBlitShader && mBlitShader->mProgramObject;

    const GLint dst_w = (GLint)mSwapChain.getWidth();
    const GLint dst_h = (GLint)mSwapChain.getHeight();

    const F64 present_start = LLTimer::getTotalSeconds();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    LLRenderTarget::sCurFBO = 0;
    glViewport(0, 0, dst_w, dst_h);
    glDrawBuffer(GL_BACK);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (shader_ready)
    {
        mBlitShader->bind();
    }

    for (LLCompositable* c : mCompositables)
    {
        if (!shader_ready)
        {
            break;
        }
        // Mailbox claim for queue-paced layers.
        c->tryAcquireNewFront();

        LLRenderTarget& front = c->frontBuffer();

        // Don't hold an outer lease while calling accessors that take
        // their own - leases don't nest. Each call below takes its own;
        // only the texture guard's lease spans the draw.
        if (!front.isComplete())
        {
            continue; // layer not allocated yet (early init)
        }

        S32 dst_x = 0, dst_y = 0;
        c->compositeOffset(dst_x, dst_y);
        const GLint w = (GLint)front.getWidth();
        const GLint h = (GLint)front.getHeight();

        // Textures are shared between contexts, FBOs aren't. The guard
        // holds the RT's shared lease across the draw and fences.
        LLScopedTexName src_tex_guard = front.getTexture(0);
        const U32 src_tex = src_tex_guard.get();
        llassert(src_tex != 0);

        // Wait on the producer's fence so we only sample finished pixels.
        // No-op if the RT didn't opt in to cross-context sync.
        front.waitFrameCompleteFence();

        // Layer rect in NDC; GL origin bottom-left matches the
        // compositeOffset convention.
        const F32 x0 = 2.f * (F32)dst_x / (F32)dst_w - 1.f;
        const F32 y0 = 2.f * (F32)dst_y / (F32)dst_h - 1.f;
        const F32 x1 = 2.f * (F32)(dst_x + w) / (F32)dst_w - 1.f;
        const F32 y1 = 2.f * (F32)(dst_y + h) / (F32)dst_h - 1.f;

        // Bind and draw the layer quad.
        static const LLStaticHashedString sBlitRect("blit_rect");
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, src_tex);
        mBlitShader->uniform4f(sBlitRect, x0, y0, x1, y1);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

        // Reverse fence: the producer waits on this before writing into
        // the buffer again.
        front.placeReadCompleteFence();
    }

    if (shader_ready)
    {
        mBlitShader->unbind();
    }

    // The layers were drawn straight into FBO 0; just swap.
    mSwapChain.presentDirect();

    const F64 present_end = LLTimer::getTotalSeconds();
    mLastPresentMs.store(
        (F32)((present_end - present_start) * 1000.0),
        std::memory_order_relaxed);

    // Present rate over a 1s window.
    ++mPresentCount;
    if (mPresentWindowStart == 0.0)
    {
        mPresentWindowStart = present_end;
        mPresentCount = 0;
    }
    else if (present_end - mPresentWindowStart >= 1.0)
    {
        mPresentFps.store((F32)(mPresentCount / (present_end - mPresentWindowStart)),
                          std::memory_order_relaxed);
        mPresentCount = 0;
        mPresentWindowStart = present_end;
    }

    // Let subscribers pace themselves to our clock.
    ++mFrameIndex;
    mOnSync(mFrameIndex);
}
