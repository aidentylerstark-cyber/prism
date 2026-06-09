/**
 * @file llresourcelease.h
 * @brief RAII lease handles for LLGPUResource synchronization.
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

#ifndef LL_LLRESOURCELEASE_H
#define LL_LLRESOURCELEASE_H

class LLGPUResource;

// RAII shared lease: many holders at once, blocks while a unique lease is
// held. Non-recursive - don't take another lease on the same resource
// while you hold one, including via subroutines.
class LLSharedLease
{
public:
    LLSharedLease() = default;                 // empty/moved-from state
    explicit LLSharedLease(LLGPUResource* res);
    ~LLSharedLease();

    LLSharedLease(LLSharedLease&& other) noexcept;
    LLSharedLease& operator=(LLSharedLease&& other) noexcept;

    LLSharedLease(const LLSharedLease&)            = delete;
    LLSharedLease& operator=(const LLSharedLease&) = delete;

    // True if this handle currently owns a lock.
    explicit operator bool() const { return mResource != nullptr; }

private:
    void release();
    LLGPUResource* mResource = nullptr;
};

// RAII unique lease: exclusive, blocks all other leases (shared and unique).
// Use for mutations: writes, name swaps, destruction prep.
class LLUniqueLease
{
public:
    LLUniqueLease() = default;
    explicit LLUniqueLease(LLGPUResource* res);
    ~LLUniqueLease();

    LLUniqueLease(LLUniqueLease&& other) noexcept;
    LLUniqueLease& operator=(LLUniqueLease&& other) noexcept;

    LLUniqueLease(const LLUniqueLease&)            = delete;
    LLUniqueLease& operator=(const LLUniqueLease&) = delete;

    explicit operator bool() const { return mResource != nullptr; }

private:
    void release();
    LLGPUResource* mResource = nullptr;
};

#endif // LL_LLRESOURCELEASE_H
