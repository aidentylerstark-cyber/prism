/**
 * @file fsradarentry.cpp
 * @brief Firestorm radar entry implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
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
 *  - RLVa calls are routed through rlvshim.h (no-op until RLVa is ported).
 *  - The OpenSim legacy properties/notes request path was removed; the base
 *    viewer has no sendAvatarNotesRequest()/APT_NOTES. Notes arrive with
 *    APT_PROPERTIES via the AgentProfile capability.
 *  - LLAvatarName::getUserNameForDisplay() (Firestorm extension) was replaced
 *    with the base viewer's LLAvatarName::getUserName().
 * TPV policy note: the hide_age handling and the agent_id == gAgentID guards
 * below are intentionally preserved verbatim; do not remove them.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsradarentry.h"

#include "fscommon.h"
#include "fsradar.h"
#include "llagent.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "rlvshim.h"

static constexpr char CAPNAME[] = "AgentProfile";

FSRadarEntry::FSRadarEntry(const LLUUID& avid)
    : mID(avid),
    mName(LLTrans::getString("AvatarNameWaiting")),
    mUserName(LLStringUtil::null),
    mDisplayName(LLStringUtil::null),
    mRange(0.f),
    mFirstSeen(time(nullptr)),
    mGlobalPos(LLVector3d(0.0, 0.0, 0.0)),
    mRegion(LLUUID::null),
    mStatus(0),
    mAge(-1),
    mIsLinden(false),
    mIgnore(false),
    mNotes(LLStringUtil::null),
    mAlertAge(false),
    mAgeAlertPerformed(false),
    mPropertiesRequested(false),
    mAvatarNameCallbackConnection()
{
    requestProperties();
    updateName();
}

FSRadarEntry::~FSRadarEntry()
{
    if (mID.notNull())
    {
        LLAvatarPropertiesProcessor::getInstance()->removeObserver(mID, this); // may try to remove null observer
    }
    if (mAvatarNameCallbackConnection.connected())
    {
        mAvatarNameCallbackConnection.disconnect();
    }
}

void FSRadarEntry::requestProperties()
{
    if (!mPropertiesRequested && mID.notNull())
    {
        if (auto region = gAgent.getRegion())
        {
            if (region->capabilitiesReceived())
            {
                LLAvatarPropertiesProcessor* processor = LLAvatarPropertiesProcessor::getInstance();
                processor->addObserver(mID, this);

                if (region->isCapabilityAvailable(CAPNAME))
                {
                    processor->sendAvatarPropertiesRequest(mID);
                }
                else
                {
                    processor->sendAvatarLegacyPropertiesRequest(mID);
                }
                mPropertiesRequested = true;
            }
        }
    }
}

void FSRadarEntry::updateName()
{
    if (mAvatarNameCallbackConnection.connected())
    {
        mAvatarNameCallbackConnection.disconnect();
    }
    mAvatarNameCallbackConnection = LLAvatarNameCache::get(mID, boost::bind(&FSRadarEntry::onAvatarNameCache, this, _1, _2));
}

void FSRadarEntry::onAvatarNameCache(const LLUUID& av_id, const LLAvatarName& av_name)
{
    if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
    {
        mUserName = av_name.getUserName();
        mDisplayName = av_name.getDisplayName();
        mName = getRadarName(av_name);
        mIsLinden = FSCommon::isLinden(av_id);
    }
    else
    {
        std::string name = getRadarName(av_name);
        mUserName = name;
        mDisplayName = name;
        mName = name;
        mIsLinden = false;
    }
}

void FSRadarEntry::processProperties(void* data, EAvatarProcessorType type)
{
    if (data)
    {
        if (type == APT_PROPERTIES)
        {
            LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
            if (avatar_data && avatar_data->agent_id == gAgentID && avatar_data->avatar_id == mID)
            {
                mStatus = avatar_data->flags;
                if (avatar_data->hide_age)
                    mAge = -2;
                else
                    mAge = (S32)((LLDate::now().secondsSinceEpoch() - (avatar_data->born_on).secondsSinceEpoch()) / 86400);
                checkAge();
                setNotes(avatar_data->notes);
            }
        }
        else if (type == APT_PROPERTIES_LEGACY)
        {
            // Prism: unlike Firestorm's, the base viewer's LLAvatarLegacyData has no
            // hide_age member, so the hide_age check cannot be replicated on this
            // path. On Second Life the AgentProfile capability is always available,
            // so this legacy UDP fallback (which only carries what the simulator
            // already sends to every legacy profile request) is effectively unused.
            LLAvatarLegacyData* avatar_data = static_cast<LLAvatarLegacyData*>(data);
            if (avatar_data && avatar_data->agent_id == gAgentID && avatar_data->avatar_id == mID)
            {
                mStatus = avatar_data->flags;
                mAge = (S32)((LLDate::now().secondsSinceEpoch() - (avatar_data->born_on).secondsSinceEpoch()) / 86400);
                checkAge();
            }
        }
    }
}

// static
std::string FSRadarEntry::getRadarName(const LLAvatarName& av_name)
{
// [RLVa:KB-FS] - Checked: 2011-06-11 (RLVa-1.3.1) | Added: RLVa-1.3.1
    if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
    {
        return RlvStrings::getAnonym(av_name);
    }
// [/RLVa:KB-FS]

    U32 fmt = gSavedSettings.getU32("RadarNameFormat");
    // if display names are enabled, allow a variety of formatting options, depending on menu selection
    if (gSavedSettings.getBOOL("UseDisplayNames"))
    {
        if (fmt == FSRADAR_NAMEFORMAT_DISPLAYNAME)
        {
            return av_name.getDisplayName();
        }
        else if (fmt == FSRADAR_NAMEFORMAT_USERNAME)
        {
            return av_name.getUserName();
        }
        else if (fmt == FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME)
        {
            if (av_name.isDisplayNameDefault())
            {
                return av_name.getUserName();
            }
            else
            {
                return llformat("%s (%s)", av_name.getDisplayName().c_str(), av_name.getUserName().c_str());
            }
        }
        else if (fmt == FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME)
        {
            if (av_name.isDisplayNameDefault())
            {
                return av_name.getUserName();
            }
            else
            {
                return llformat("%s (%s)", av_name.getUserName().c_str(), av_name.getDisplayName().c_str());
            }
        }
    }

    // else use legacy name lookups
    return av_name.getUserName();
}

void FSRadarEntry::checkAge()
{
    mAlertAge = (mAge > -1 && mAge <= gSavedSettings.getS32("RadarAvatarAgeAlertValue"));
    if (!mAlertAge || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
    {
        mAgeAlertPerformed = true;
    }
}

void FSRadarEntry::setNotes(std::string_view notes)
{
    mNotes = notes;
    LLStringUtil::trim(mNotes);
}
