/**
 * @file llviewernetworkthread.cpp
 * @brief Viewer message system's thread
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2025, Linden Research, Inc.
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

#include "llviewernetworkthread.h"

#include "llapp.h"
//#include "llwatchdog.h"


LLViewerNetworkThread::LLViewerNetworkThread() :
    LLThread("NETWORK"),
    mStopping(false),
    mWorkQueue("NETWORK", 128)
{
    //mNetworkTimeout = new LLWatchdogTimeout();
    start();
}

LLViewerNetworkThread::~LLViewerNetworkThread()
{
    stop();
    //delete mNetworkTimeout;
}

void LLViewerNetworkThread::stop()
{
    mStopping = true;
}

void LLViewerNetworkThread::run()
{
    // todo: move watchdog to common then readd it.
    //mNetworkTimeout->setTimeout(60.f);
    //mNetworkTimeout->start();
    //mNetworkTimeout->ping("idling");
    while (!mStopping && !mWorkQueue.isClosed())
    {
        //mNetworkTimeout->ping("working");
        LLApp::instance()->processNetwork();

        //mNetworkTimeout->ping("queue");
        mWorkQueue.runPending();

        //mNetworkTimeout->ping("idling");
        ms_sleep(10);
    }
    mWorkQueue.runPending();
    //mNetworkTimeout->stop();
}

void LLViewerNetworkThread::postWork(const LL::WorkQueue::Work& work)
{
    LLViewerNetworkThread* self = getInstance();
    if (self)
    {
        self->mWorkQueue.post(work);
    }
    else
    {
        llassert(false); //LLViewerNetworkThread does not exist
    }
}

//EOF
