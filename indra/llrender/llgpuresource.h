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
// Provides two kinds of RAII lease: shared (many readers) and unique (one
// writer). Subclasses can override the acquire/release hooks to place GL
// fences at the lease boundary; the defaults do nothing, so resources that
// don't need cross-context fences pay nothing.
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
    // be leased again.
    LLGPUResource(LLGPUResource&& other) noexcept = default;
    LLGPUResource& operator=(LLGPUResource&&)     = delete;

    LLSharedLease getSharedLease() const { return LLSharedLease(const_cast<LLGPUResource*>(this)); }
    LLUniqueLease getUniqueLease()       { return LLUniqueLease(this); }

protected:
    // Lease lifecycle hooks, called with the lock held. Override the ones
    // you care about; the defaults do nothing.
    virtual void onSharedAcquire() {}
    virtual void onSharedRelease() {}
    virtual void onUniqueAcquire() {}
    virtual void onUniqueRelease() {}

private:
    friend class LLSharedLease;
    friend class LLUniqueLease;
    mutable std::unique_ptr<std::shared_mutex> mLeaseMutex;
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
