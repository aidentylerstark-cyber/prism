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

#include <memory>
#include <shared_mutex>

// Base class for GPU resources that need cross-thread synchronization.
//
// Provides two kinds of RAII lease:
//   - shared (many concurrent holders, read-only access)
//   - unique (exclusive, for mutation)
//
// Backed by std::shared_mutex. Subclasses override the on*Acquire/Release
// hooks to inject GL fences at the lease boundary when GPU-side ordering
// needs to follow CPU-side serialization -- e.g. a worker context's writes
// must be GPU-visible before a different context reads. Defaults are no-op,
// so resources that don't need cross-context fences pay nothing.
//
// Leases follow std::shared_mutex semantics: non-recursive, same-thread
// release. A thread that holds a lease must not acquire another lease on
// the same resource (deadlock).
class LLGPUResource
{
public:
    LLGPUResource() : mLeaseMutex(std::make_unique<std::shared_mutex>()) {}
    virtual ~LLGPUResource();

    LLGPUResource(const LLGPUResource&)            = delete;
    LLGPUResource& operator=(const LLGPUResource&) = delete;

    // Movable: the mutex hides behind a unique_ptr so the default move
    // ctor works. After a move the source's mLeaseMutex is null -- further
    // lease ops on it are UB. Only safe to move when no leases are live;
    // in practice that's container setup time, never mid-render. Needed so
    // vector<LLRenderTarget> et al. can reallocate.
    LLGPUResource(LLGPUResource&& other) noexcept = default;
    LLGPUResource& operator=(LLGPUResource&&)     = delete;

    LLSharedLease getSharedLease() const { return LLSharedLease(const_cast<LLGPUResource*>(this)); }
    LLUniqueLease getUniqueLease()       { return LLUniqueLease(this); }

protected:
    // Hooks called by the lease lifecycle. Defaults are no-op. Subclasses
    // override the ones they care about. Called with the appropriate lock
    // held (shared lock for shared hooks, exclusive lock for unique hooks).
    virtual void onSharedAcquire() {}
    virtual void onSharedRelease() {}
    virtual void onUniqueAcquire() {}
    virtual void onUniqueRelease() {}

private:
    friend class LLSharedLease;
    friend class LLUniqueLease;
    mutable std::unique_ptr<std::shared_mutex> mLeaseMutex;
};

// Guarded GL texture name. Returned by accessors that would otherwise be
// snapshot-return -- the lease lives in this guard, keyed to the caller's
// scope. Use .get() to read the value; the guard's lifetime is what holds
// the lock.
//
// Common safe patterns:
//   glBindTexture(target, img->getTexName().get());     // expression scope
//   auto name = img->getTexName();                       // statement scope
//   ... use name.get() ...
//
// Unsafe (don't): assigning .get() into an LLGLuint local -- the guard temp
// destructs at the ;, leaving you using the value without the lock.
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
