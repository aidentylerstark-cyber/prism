/**
 * @file fsradarmenu.cpp
 * @brief Menu used by Firestorm radar
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * Adapted for Prism from the Firestorm Viewer sources:
 *  - Contact sets, netmap mark colors, face-towards-avatar, script info,
 *    derender and estate/land moderation entries were removed (their
 *    subsystems / LLAvatarActions extensions are not in the base viewer).
 *  - Track/TeleportTo/ZoomIn go through FSRadar directly instead of the
 *    Firestorm LLAvatarActions extensions.
 *  - Render mode persistence for non-rezzed avatars uses the base viewer's
 *    LLRenderMuteList (same mechanism as the official avatar render
 *    settings floater) instead of FSAvatarRenderPersistence.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

// libs
#include "llmenugl.h"

#include "fsradarmenu.h"

// newview
#include "fsradar.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llcallingcard.h"          // for LLAvatarTracker
#include "lllogchat.h"
#include "llmutelist.h"             // for LLRenderMuteList
#include "llviewermenu.h"           // for gMenuHolder, is_agent_mappable
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "rlvshim.h"

namespace FSFloaterRadarMenu
{

FSRadarMenu gFSRadarMenu;

//== NearbyMenu ===============================================================

LLContextMenu* FSRadarMenu::createMenu()
{
    // set up the callbacks for all of the avatar menu items
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    if ( mUUIDs.size() == 1 )
    {
        // Set up for one person selected menu

        const LLUUID& id = mUUIDs.front();
        registrar.add("Avatar.Profile",                         boost::bind(&LLAvatarActions::showProfile,                  id));
        registrar.add("Avatar.AddFriend",                       boost::bind(&LLAvatarActions::requestFriendshipDialog,      id));
        registrar.add("Avatar.RemoveFriend",                    boost::bind(&LLAvatarActions::removeFriendDialog,           id));
        registrar.add("Avatar.IM",                              boost::bind(&LLAvatarActions::startIM,                      id));
        registrar.add("Avatar.Call",                            boost::bind(&LLAvatarActions::startCall,                    id));
        registrar.add("Avatar.OfferTeleport",                   boost::bind(&FSRadarMenu::offerTeleport,                    this));
        registrar.add("Avatar.TeleportRequest",                 boost::bind(&LLAvatarActions::teleportRequest,              id));
        registrar.add("Avatar.GroupInvite",                     boost::bind(&LLAvatarActions::inviteToGroup,                id));
        registrar.add("Avatar.ShowOnMap",                       boost::bind(&LLAvatarActions::showOnMap,                    id));
        registrar.add("Avatar.Share",                           boost::bind(&LLAvatarActions::share,                        id));
        registrar.add("Avatar.Pay",                             boost::bind(&LLAvatarActions::pay,                          id));
        registrar.add("Avatar.BlockUnblock",                    boost::bind(&LLAvatarActions::toggleBlock,                  id));
        registrar.add("Avatar.ZoomIn",                          boost::bind(&FSRadarMenu::onZoomInMenuItemClick,            this));
        registrar.add("Avatar.Calllog",                         boost::bind(&LLAvatarActions::viewChatHistory,              id));
        registrar.add("Nearby.People.TeleportToAvatar",         boost::bind(&FSRadarMenu::teleportToAvatar,                 this));
        registrar.add("Nearby.People.TrackAvatar",              boost::bind(&FSRadarMenu::onTrackAvatarMenuItemClick,       this));
        registrar.add("Nearby.People.SetRenderMode",            boost::bind(&FSRadarMenu::onSetRenderMode,                  this, _2));

        enable_registrar.add("Avatar.EnableItem",               boost::bind(&FSRadarMenu::enableContextMenuItem,            this, _2));
        enable_registrar.add("Avatar.CheckItem",                boost::bind(&FSRadarMenu::checkContextMenuItem,             this, _2));
        enable_registrar.add("Avatar.VisibleZoomIn",            boost::bind(&FSRadarMenu::canZoomIn,                        this));
        enable_registrar.add("Nearby.People.CheckRenderMode",   boost::bind(&FSRadarMenu::checkSetRenderMode,               this, _2));

        // create the context menu from the XUI
        return createFromFile("menu_fs_radar.xml");
    }
    else
    {
        // Set up for multi-selected People

        // registrar.add("Avatar.AddFriend",                    boost::bind(&LLAvatarActions::requestFriendshipDialog,              mUUIDs)); // *TODO: unimplemented
        registrar.add("Avatar.IM",                              boost::bind(&LLAvatarActions::startConference,                      mUUIDs, LLUUID::null));
        registrar.add("Avatar.Call",                            boost::bind(&LLAvatarActions::startAdhocCall,                       mUUIDs, LLUUID::null));
        registrar.add("Avatar.OfferTeleport",                   boost::bind(&FSRadarMenu::offerTeleport,                            this));
        registrar.add("Avatar.RemoveFriend",                    boost::bind(&LLAvatarActions::removeFriendsDialog,                  mUUIDs));
        // registrar.add("Avatar.Share",                        boost::bind(&LLAvatarActions::startIM,                              mUUIDs)); // *TODO: unimplemented
        // registrar.add("Avatar.Pay",                          boost::bind(&LLAvatarActions::pay,                                  mUUIDs)); // *TODO: unimplemented
        registrar.add("Nearby.People.SetRenderMode",            boost::bind(&FSRadarMenu::onSetRenderMode,                          this, _2));

        enable_registrar.add("Avatar.EnableItem",               boost::bind(&FSRadarMenu::enableContextMenuItem,                    this, _2));

        // create the context menu from the XUI
        return createFromFile("menu_fs_radar_multiselect.xml");
    }
}

bool FSRadarMenu::enableContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();

    // Note: can_block and can_delete is used only for one person selected menu
    // so we don't need to go over all uuids.

    if (item == std::string("can_block"))
    {
        const LLUUID& id = mUUIDs.front();
        return LLAvatarActions::canBlock(id);
    }
    else if (item == std::string("can_add"))
    {
        // We can add friends if:
        // - there are selected people
        // - and there are no friends among selection yet.

        //EXT-7389 - disable for more than 1
        if(mUUIDs.size() > 1)
        {
            return false;
        }

        bool result = (mUUIDs.size() > 0);

        uuid_vec_t::const_iterator
            id = mUUIDs.begin(),
            uuids_end = mUUIDs.end();

        for (;id != uuids_end; ++id)
        {
            if ( LLAvatarActions::isFriend(*id) )
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == std::string("can_delete"))
    {
        // We can remove friends if:
        // - there are selected people
        // - and there are only friends among selection.

        bool result = (mUUIDs.size() > 0);

        uuid_vec_t::const_iterator
            id = mUUIDs.begin(),
            uuids_end = mUUIDs.end();

        for (;id != uuids_end; ++id)
        {
            if ( !LLAvatarActions::isFriend(*id) )
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == std::string("can_call"))
    {
        return LLAvatarActions::canCall();
    }
    else if (item == std::string("can_show_on_map"))
    {
        const LLUUID& id = mUUIDs.front();

        return (LLAvatarTracker::instance().isBuddyOnline(id) && is_agent_mappable(id))
                    || gAgent.isGodlike();
    }
    else if(item == std::string("can_offer_teleport"))
    {
        return LLAvatarActions::canOfferTeleport(mUUIDs);
    }
    else if(item == std::string("can_request_teleport"))
    {
        // Prism: the base viewer has no LLAvatarActions::canRequestTeleport();
        // allow the request for single selections like the stock people menus do.
        return (mUUIDs.size() == 1);
    }
    else if (item == std::string("can_open_inventory"))
    {
        return (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV));
    }
    else if (item == std::string("can_pay"))
    {
        const LLUUID& id = mUUIDs.front();
        return RlvActions::canPayAvatar(id);
    }
    else if (item == std::string("can_callog"))
    {
        return LLLogChat::isTranscriptExist(mUUIDs.front());
    }
    return false;
}

bool FSRadarMenu::checkContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();
    const LLUUID& id = mUUIDs.front();

    if (item == std::string("is_blocked"))
    {
        return LLAvatarActions::isBlocked(id);
    }

    return false;
}

void FSRadarMenu::offerTeleport()
{
    // boost::bind cannot recognize overloaded method LLAvatarActions::offerTeleport(),
    // so we have to use a wrapper.
    LLAvatarActions::offerTeleport(mUUIDs);
}

void FSRadarMenu::teleportToAvatar()
// Will only work for avatars within radar range.
{
    FSRadar::getInstance()->teleportToAvatar(mUUIDs.front());
}

// Ansariel: Avatar tracking feature
void FSRadarMenu::onTrackAvatarMenuItemClick()
{
    FSRadar::getInstance()->startTracking(mUUIDs.front());
}

void FSRadarMenu::onZoomInMenuItemClick()
{
    const LLUUID& id = mUUIDs.front();
    std::string name;
    if (auto entry = FSRadar::getInstance()->getEntry(id))
    {
        name = entry->getName();
    }
    FSRadar::getInstance()->zoomAvatar(id, name);
}

bool FSRadarMenu::canZoomIn()
{
    return gObjectList.findObject(mUUIDs.front()) != nullptr;
}

void FSRadarMenu::onSetRenderMode(const LLSD& userdata)
{
    LLVOAvatar::VisualMuteSettings render_setting;
    U32 mode = userdata.asInteger();
    switch (mode)
    {
        case 0:
            render_setting = LLVOAvatar::AV_RENDER_NORMALLY;
            break;
        case 1:
            render_setting = LLVOAvatar::AV_DO_NOT_RENDER;
            break;
        case 2:
            render_setting = LLVOAvatar::AV_ALWAYS_RENDER;
            break;
        default:
            LL_WARNS() << "Unknown value for visual mute settings: " << mode << LL_ENDL;
            return;
    }

    bool needs_culling = false;
    for (uuid_vec_t::const_iterator it = mUUIDs.begin(); it != mUUIDs.end(); ++it)
    {
        const LLUUID& avatar_id = *it;

        LLVOAvatar *avatarp = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(avatar_id));
        if (avatarp)
        {
            avatarp->setVisualMuteSettings(render_setting);
            needs_culling = true;
        }
        else
        {
            // Prism: persist via the base viewer's render mute list (same as
            // the official avatar render settings floater).
            LLRenderMuteList::getInstance()->saveVisualMuteSetting(avatar_id, S32(render_setting));
        }
    }

    if (needs_culling)
    {
        LLVOAvatar::cullAvatarsByPixelArea();
    }
}

bool FSRadarMenu::checkSetRenderMode(const LLSD& userdata)
{
    LLVOAvatar::VisualMuteSettings render_setting;
    U32 mode = userdata.asInteger();
    switch (mode)
    {
        case 0:
            render_setting = LLVOAvatar::AV_RENDER_NORMALLY;
            break;
        case 1:
            render_setting = LLVOAvatar::AV_DO_NOT_RENDER;
            break;
        case 2:
            render_setting = LLVOAvatar::AV_ALWAYS_RENDER;
            break;
        default:
            LL_WARNS() << "Unknown value for visual mute settings: " << mode << LL_ENDL;
            return false;
    }

    if (LLVOAvatar* avatarp = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(mUUIDs.front())))
    {
        return avatarp->getVisualMuteSettings() == render_setting;
    }

    return LLRenderMuteList::getInstance()->getSavedVisualMuteSetting(mUUIDs.front()) == S32(render_setting);
}

} // namespace FSFloaterRadarMenu
