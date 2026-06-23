/**
 * @file lltestsquarecompositable.cpp
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

#include "lltestsquarecompositable.h"

#include "llcompositor.h"
#include "llrender.h"
#include "llimagegl.h"
#include "llwindow.h"

#include <vector>

LLTestSquareCompositable::LLTestSquareCompositable(const std::string& name,
                                                   U8 r, U8 g, U8 b,
                                                   U32 interval, S32 x, S32 y,
                                                   LLWindow* window,
                                                   U32 size)
:   LLThread("TestSquare:" + name),
    mName(name),
    mColor{r, g, b},
    mInterval(interval ? interval : 1),
    mX(x),
    mY(y),
    mSize(size),
    mWindow(window),
    // Created on the calling thread (OS main, GL context current);
    // made current on the producer thread in run().
    mContext(window->createSharedContext())
{
}

LLTestSquareCompositable::~LLTestSquareCompositable()
{
    disconnect();
}

void LLTestSquareCompositable::connect(LLCompositor& compositor)
{
    mCompositor = &compositor;
    start();
}

void LLTestSquareCompositable::disconnect()
{
    if (!isStopped())
    {
        setQuitting();
        if (mCompositor) mCompositor->wakeSyncWaiters(); // break our waitForPresent
        shutdown(); // joins
    }
}

void LLTestSquareCompositable::run()
{
    // Producer-thread setup: make our context current, bring up gGL, then
    // allocate our GL resources. The RT's FBO lives in this context - the
    // compositor only ever touches the shared color texture.
    mWindow->makeContextCurrent(mContext);
    gGL.init(false);

    mRT.allocate(mSize, mSize, GL_RGBA, /*depth=*/false);
    mRT.setNeedsCrossContextSync(true);

    // First frame so the layer shows up before the first tick.
    paint();
    if (LLImageGL* sync = mRT.getColorAttachmentImage())
    {
        sync->placeFrameCompleteFence(); // also flushes
    }
    else
    {
        glFlush();
    }
    produceFrame();

    while (!isQuitting())
    {
        // Pace to the compositor's present clock; the should_stop predicate +
        // wakeSyncWaiters (on disconnect) break the wait promptly.
        U64 tick = mCompositor->waitForPresent(mLastTick + 1, [this]{ return isQuitting(); });
        if (tick <= mLastTick || isQuitting())
        {
            break; // interrupted (disconnect / shutdown)
        }
        mLastTick = tick;

        if (tick % mInterval == 0)
        {
            // Don't overwrite pixels the compositor may still be
            // copying - GPU-side wait on its read-complete fence.
            if (LLImageGL* sync = mRT.getColorAttachmentImage())
            {
                sync->waitReadCompleteFence();
            }

            mStep = (mStep + 3) % mSize;
            paint();

            // Publish: the compositor waits on this fence before sampling.
            // placeFrameCompleteFence flushes so the other context sees it.
            if (LLImageGL* sync = mRT.getColorAttachmentImage())
            {
                sync->placeFrameCompleteFence();
            }
            else
            {
                glFlush();
            }
            produceFrame();
        }
    }

    // Tear down on this thread - the context has to be destroyed where
    // it's current.
    mRT.release();
    gGL.shutdown();
    mWindow->destroySharedContext(mContext);
    mContext = nullptr;
}

void LLTestSquareCompositable::paint()
{
    // Fill the RT's color texture: solid base color with a bright
    // vertical bar at the current sweep position (wraps around).
    const U8 r = mColor[0];
    const U8 g = mColor[1];
    const U8 b = mColor[2];

    // Bar pixels lighten toward white.
    auto lighten = [](U8 c) { return (U8)(c + ((255 - c) * 3) / 4); };
    const U8 br = lighten(r);
    const U8 bg = lighten(g);
    const U8 bb = lighten(b);

    const U32 kBarWidth = 12;

    std::vector<U8> pixels(mSize * mSize * 4);
    for (U32 y = 0; y < mSize; ++y)
    {
        for (U32 x = 0; x < mSize; ++x)
        {
            // Distance from the bar start, wrapping at the edge.
            const U32 dist = (x + mSize - mStep) % mSize;
            const bool in_bar = dist < kBarWidth;
            const size_t i = (y * mSize + x) * 4;
            pixels[i + 0] = in_bar ? br : r;
            pixels[i + 1] = in_bar ? bg : g;
            pixels[i + 2] = in_bar ? bb : b;
            pixels[i + 3] = 255;
        }
    }

    U32 tex = mRT.getTexture(0);
    gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)mSize, (GLsizei)mSize,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
}
