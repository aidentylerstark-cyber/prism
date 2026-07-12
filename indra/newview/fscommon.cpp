/**
 * @file fscommon.cpp
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
 * radar port:
 *  - isLinden() uses LLCacheName::getFullName() because the Firestorm
 *    getFirstLastName() extension does not exist in the base viewer.
 *  - isFilterEditorKeyCombo() is hardcoded to Ctrl-F (the Firestorm setting
 *    FSSelectLocalSearchEditorOnShortcut is not ported).
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fscommon.h"

#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "llcachename.h"
#include "llchat.h"
#include "llnotificationmanager.h"

#include <sstream>

static const std::string LL_LINDEN = "Linden";
static const std::string LL_MOLE = "Mole";
static const std::string LL_PRODUCTENGINE = "ProductEngine";
static const std::string LL_SCOUT = "Scout";
static const std::string LL_TESTER = "Tester";

void FSCommon::report_to_nearby_chat(std::string_view message)
{
    LLChat chat;
    chat.mText = message;
    chat.mSourceType = CHAT_SOURCE_SYSTEM;
    LLNotificationsUI::LLNotificationManager::instance().onChat(chat, LLSD());
}

std::string FSCommon::format_string(std::string text, const LLStringUtil::format_map_t& args)
{
    LLStringUtil::format(text, args);
    return text;
}

bool FSCommon::isLinden(const LLUUID& av_id)
{
    std::string first_name, last_name;
    LLAvatarName av_name;
    if (LLAvatarNameCache::get(av_id, &av_name))
    {
        std::istringstream full_name(av_name.getUserName());
        full_name >> first_name >> last_name;
    }
    else if (gCacheName)
    {
        std::string full_name;
        if (gCacheName->getFullName(av_id, full_name))
        {
            std::istringstream name_stream(full_name);
            name_stream >> first_name >> last_name;
        }
    }

    return (last_name == LL_LINDEN ||
            last_name == LL_MOLE ||
            last_name == LL_PRODUCTENGINE ||
            last_name == LL_SCOUT ||
            last_name == LL_TESTER);
}

bool FSCommon::isFilterEditorKeyCombo(KEY key, MASK mask)
{
    return (mask == MASK_CONTROL && key == 'F');
}
