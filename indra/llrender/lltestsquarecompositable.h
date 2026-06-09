/**
 * @file lltestsquarecompositable.h
 * @brief Threaded solid-color test layer for compositor bring-up.
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

#ifndef LL_LLTESTSQUARECOMPOSITABLE_H
#define LL_LLTESTSQUARECOMPOSITABLE_H

#include "llcompositable.h"
#include "llrendertarget.h"
#include "llthread.h"
#include "llthreadsafequeue.h"

#include <boost/signals2.hpp>

class LLCompositor;
class LLWindow;

// Compositor bring-up test layer, and a miniature model of the real
// producer architecture: a dedicated thread with its own shared GL context
// paints a sweeping bar into its own RT and publishes it through the
// cross-context fence pair, paced by the compositor's sync signal
// delivered over a queue.
class LLTestSquareCompositable : public LLCompositable, public LLThread
{
public:
    // color: RGB. interval: repaint every Nth sync. (x, y): destination
    // offset in the swap chain image, GL origin. window: source of the
    // shared GL context (created in the ctor on the calling thread,
    // which must have a current GL context).
    LLTestSquareCompositable(const std::string& name,
                             U8 r, U8 g, U8 b,
                             U32 interval, S32 x, S32 y,
                             LLWindow* window,
                             U32 size = 100);
    ~LLTestSquareCompositable() override;

    // Subscribe to the compositor's sync signal and start the producer
    // thread. Call on the compositor's thread.
    void connect(LLCompositor& compositor);

    // Unsubscribe, wake + join the thread (it releases its GL resources
    // and destroys its context on the way out).
    void disconnect();

    // - LLCompositable -----------------------------------------------
    LLRenderTarget& frontBuffer() override { return mRT; }
    void compositeOffset(S32& x, S32& y) const override { x = mX; y = mY; }
    std::string compositableName() const override { return mName; }

protected:
    // - LLThread -------------------------------------------------------
    // Producer loop: make the shared context current, allocate the RT,
    // then consume sync ticks until the quit sentinel arrives.
    void run() override;

private:
    // Fill the RT's color texture: solid color + sweep bar at mStep.
    void paint();

    static constexpr U64 kQuitTick = ~U64(0);

    LLRenderTarget mRT;
    std::string mName;
    U8   mColor[3];
    U32  mInterval;
    S32  mX;
    S32  mY;
    U32  mSize;
    U32  mStep = 0;    // sweep-bar position; advances per repaint

    LLWindow* mWindow;
    void*     mContext;

    // Sync ticks from the compositor. Bounded, and the handler uses
    // tryPush, so a slow square never blocks the compositor - extra
    // ticks just drop.
    LLThreadSafeQueue<U64> mTicks{64};

    boost::signals2::connection mSyncConnection;
};

#endif // LL_LLTESTSQUARECOMPOSITABLE_H
