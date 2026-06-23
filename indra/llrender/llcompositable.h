/**
 * @file llcompositable.h
 * @brief Pure-virtual interface for compositor layer producers.
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

#ifndef LL_LLCOMPOSITABLE_H
#define LL_LLCOMPOSITABLE_H

#include "stdtypes.h"
#include "lltimer.h"

#include <atomic>
#include <string>

class LLRenderTarget;

// One layer in the compositor's stack. A compositable owns its front buffer
// (an LLRenderTarget); the compositor takes a shared lease on it and samples
// it during present.
//
// Neither side blocks the other: frames hand over via tryAcquireNewFront,
// and the per-RT fence pair orders the GPU reads and writes.
class LLCompositable
{
public:
    virtual ~LLCompositable() = default;

    // The front buffer the compositor will draw from. Doesn't take a
    // lease - the compositor handles that.
    virtual LLRenderTarget& frontBuffer() = 0;

    // Destination offset (pixels, GL origin = bottom-left) where this
    // layer's front buffer lands in the swap chain image. Fullscreen
    // layers keep the 0,0 default.
    virtual void compositeOffset(S32& x, S32& y) const { x = 0; y = 0; }

    // Display name for debug overlays (Show Render Info).
    virtual std::string compositableName() const { return "Layer"; }

    // Called by the compositor before reading frontBuffer(). Mailbox-paced
    // layers claim the latest published buffer here and return true when
    // they got a fresh one; single-buffer layers keep the no-op default.
    virtual bool tryAcquireNewFront() { return false; }

    // Producer-side frame metrics: wall-clock time between one publish and
    // the next, written by produceFrame() and read by the stats overlay.
    F32 lastFrameMs() const { return mFrameMs.load(std::memory_order_relaxed); }

protected:
    // Producers call this when a frame is complete - stamps the cycle
    // metrics.
    void produceFrame()
    {
        const F64 now = LLTimer::getTotalSeconds();
        if (mLastProduceTime > 0.0)
        {
            mFrameMs.store((F32)((now - mLastProduceTime) * 1000.0),
                           std::memory_order_relaxed);
        }
        mLastProduceTime = now; // producer-thread-only
    }

private:
    std::atomic<F32> mFrameMs{0.f};
    F64              mLastProduceTime = 0.0;
};

#endif // LL_LLCOMPOSITABLE_H
