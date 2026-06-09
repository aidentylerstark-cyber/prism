/**
 * @file llgpuresource.cpp
 * @brief LLGPUResource + lease implementation.
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

#include "llgpuresource.h"

LLGPUResource::~LLGPUResource()
{
    // mLeaseMutex is null if this resource was moved-from.
    if (!mLeaseMutex)
    {
        return;
    }
    // Verify no leases are live.
    if (mLeaseMutex->try_lock())
    {
        mLeaseMutex->unlock();
    }
    else
    {
        llassert(false); // a lease holder outlived this resource
    }
}

// - LLSharedLease ----------------------------------------------------------

LLSharedLease::LLSharedLease(LLGPUResource* res)
    : mResource(res)
{
    if (mResource)
    {
        mResource->mLeaseMutex->lock_shared();
        mResource->onSharedAcquire();
    }
}

LLSharedLease::~LLSharedLease()
{
    release();
}

LLSharedLease::LLSharedLease(LLSharedLease&& other) noexcept
    : mResource(other.mResource)
{
    other.mResource = nullptr;
}

LLSharedLease& LLSharedLease::operator=(LLSharedLease&& other) noexcept
{
    if (this != &other)
    {
        release();
        mResource = other.mResource;
        other.mResource = nullptr;
    }
    return *this;
}

void LLSharedLease::release()
{
    if (mResource)
    {
        mResource->onSharedRelease();
        mResource->mLeaseMutex->unlock_shared();
        mResource = nullptr;
    }
}

// - LLUniqueLease ----------------------------------------------------------

LLUniqueLease::LLUniqueLease(LLGPUResource* res)
    : mResource(res)
{
    if (mResource)
    {
        mResource->mLeaseMutex->lock();
        mResource->onUniqueAcquire();
    }
}

LLUniqueLease::~LLUniqueLease()
{
    release();
}

LLUniqueLease::LLUniqueLease(LLUniqueLease&& other) noexcept
    : mResource(other.mResource)
{
    other.mResource = nullptr;
}

LLUniqueLease& LLUniqueLease::operator=(LLUniqueLease&& other) noexcept
{
    if (this != &other)
    {
        release();
        mResource = other.mResource;
        other.mResource = nullptr;
    }
    return *this;
}

void LLUniqueLease::release()
{
    if (mResource)
    {
        mResource->onUniqueRelease();
        mResource->mLeaseMutex->unlock();
        mResource = nullptr;
    }
}
