/**
 * @file llgpuresource.h
 * @brief Base class for GPU-bound resources with lease-based synchronization.
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

#ifndef LL_LLGPURESOURCE_H
#define LL_LLGPURESOURCE_H

#include "llresourcelease.h"
#include "llgltypes.h"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <utility>

// Base class for GPU resources that need cross-thread synchronization.
//
// Provides two kinds of RAII lease: shared (many readers) and unique (one
// writer) - a plain reader/writer lock, nothing more. Leasing is opt-in
// (setLeaseEnabled): only resources actually shared across threads take the
// lock; everything else gets an empty lease and pays nothing on the bind hot
// path. Cross-context GL fences are placed/waited explicitly by callers (see
// LLImageGL), not by the lease.
//
// Leases follow std::shared_mutex rules: non-recursive, so don't take a
// second lease on the same resource while you already hold one.
class LLGPUResource
{
public:
    LLGPUResource() : mLeaseMutex(std::make_unique<std::shared_mutex>()) {}
    virtual ~LLGPUResource();

    LLGPUResource(const LLGPUResource&)            = delete;
    LLGPUResource& operator=(const LLGPUResource&) = delete;

    // Movable so containers of these can reallocate. Only move when no
    // leases are live; a moved-from resource has a null mutex and can't
    // be leased again. Spelled out because std::atomic isn't movable.
    LLGPUResource(LLGPUResource&& other) noexcept
        : mLeaseMutex(std::move(other.mLeaseMutex)),
          mLeaseEnabled(other.mLeaseEnabled.load(std::memory_order_relaxed)) {}
    LLGPUResource& operator=(LLGPUResource&&)     = delete;

    // Opt-in: a resource that hasn't enabled leasing returns an empty lease,
    // so the lock is skipped entirely.
    LLSharedLease getSharedLease() const
    {
        return LLSharedLease(mLeaseEnabled.load(std::memory_order_acquire)
                             ? const_cast<LLGPUResource*>(this) : nullptr);
    }
    LLUniqueLease getUniqueLease()
    {
        return LLUniqueLease(mLeaseEnabled.load(std::memory_order_acquire) ? this : nullptr);
    }

protected:
    // Enable leasing for this resource. Off by default - flip it on once the
    // resource is actually handed across threads (e.g. an off-thread texture
    // upload), before the first cross-thread lease.
    void setLeaseEnabled(bool b) { mLeaseEnabled.store(b, std::memory_order_release); }

private:
    friend class LLSharedLease;
    friend class LLUniqueLease;
    mutable std::unique_ptr<std::shared_mutex> mLeaseMutex;
    std::atomic<bool> mLeaseEnabled{false};
};

// Guarded GL texture name - the guard holds a shared lease for as long as
// it lives. Use .get() to read the value.
//
// Safe:
//   glBindTexture(target, img->getTexName().get());
//   auto name = img->getTexName(); ... name.get() ...
//
// Unsafe: stashing .get() into a plain LLGLuint - the guard temp dies at
// the semicolon and you're using the value without the lock.
class LLScopedTexName
{
public:
    LLScopedTexName() = default;
    LLScopedTexName(LLSharedLease lease, LLGLuint name)
        : mLease(std::move(lease)), mName(name) {}

    LLScopedTexName(const LLScopedTexName&)            = delete;
    LLScopedTexName& operator=(const LLScopedTexName&) = delete;
    LLScopedTexName(LLScopedTexName&&)                 = default;
    LLScopedTexName& operator=(LLScopedTexName&&)      = default;

    LLGLuint get() const { return mName; }
    explicit operator bool() const { return mName != 0; }

private:
    LLSharedLease mLease;
    LLGLuint      mName = 0;
};

#endif // LL_LLGPURESOURCE_H
