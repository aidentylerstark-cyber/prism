/**
 * @file fscommon.h
 * @brief Central class for common used functions in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 *
 * Adapted for Prism from the Firestorm Viewer sources. This is a minimal
 * subset of Firestorm's FSCommon containing only the helpers needed by the
 * radar port.
 * $/LicenseInfo$
 */

#ifndef FS_COMMON_H
#define FS_COMMON_H

#include "indra_constants.h"
#include "llstring.h"
#include "lluuid.h"

constexpr F64 AVATAR_UNKNOWN_Z_OFFSET = -1.0; // Const value for avatars at unknown height
constexpr F32 AVATAR_UNKNOWN_RANGE = -1.f;

namespace FSCommon
{
    void report_to_nearby_chat(std::string_view message);

    std::string format_string(std::string text, const LLStringUtil::format_map_t& args);

    bool isLinden(const LLUUID& av_id);

    bool isFilterEditorKeyCombo(KEY key, MASK mask);
};

#endif // FS_COMMON_H
