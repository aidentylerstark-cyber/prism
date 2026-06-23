/**
 * @file llviewerthread.h
 * @brief Dedicated thread that runs the viewer's frame loop.
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

#ifndef LL_LLVIEWERTHREAD_H
#define LL_LLVIEWERTHREAD_H

#include "llthread.h"

class LLWindow;

// Dedicated thread that runs the viewer's frame loop - the work that
// used to live on the OS main thread. It owns its own GL context,
// shared with the primary one, so textures are visible to both. The
// OS main thread keeps the message pump and the compositor.
//
// Construct and start() this on the OS main thread once the window's
// context is up. On shutdown, call LLCompositor::requestShutdown() first
// to unblock any producer waiting on a present, then requestStop() and
// join via shutdown().
class LLViewerThread : public LLThread
{
public:
    LLViewerThread(LLWindow* window);
    ~LLViewerThread() override;

    // Flag the thread for shutdown. The loop checks isQuitting() between
    // ticks. If the tick may be parked waiting on a present, call
    // LLCompositor::requestShutdown() first to unstick it, then join via
    // shutdown().
    void requestStop() { setQuitting(); }

protected:
    void run() override;

private:
    LLWindow* mWindow;
    void*     mContext;
};

#endif // LL_LLVIEWERTHREAD_H
