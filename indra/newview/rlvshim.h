/**
 * @file rlvshim.h
 * @brief No-op stand-ins for the RLVa API used by code ported from Firestorm.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Prism Viewer Source Code
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
 * $/LicenseInfo$
 *
 * Prism does not (yet) ship RLVa. Ported Firestorm code keeps its original
 * RLVa call sites intact so that a future real RLVa port only needs to swap
 * this include for the real rlvactions.h/rlvhandler.h/rlvcommon.h headers.
 * Every shim below answers with the "no restriction active" result:
 *  - RlvActions::canShowNearbyAgents()          -> true
 *  - RlvActions::canPayAvatar()                 -> true
 *  - gRlvHandler.hasBehaviour(...)              -> false
 *  - RlvStrings::getAnonym()/getString()        -> placeholder strings
 */

#ifndef RLV_SHIM_H
#define RLV_SHIM_H

#include <string>
#include <string_view>

class LLAvatarName;
class LLUUID;

enum ERlvBehaviour
{
    RLV_BHVR_SHOWINV,
    RLV_BHVR_SHOWNAMES,
    RLV_BHVR_COUNT
};

class RlvActions
{
public:
    static bool canShowNearbyAgents()           { return true; }
    static bool canPayAvatar(const LLUUID&)     { return true; }
};

class RlvStrings
{
public:
    static std::string getAnonym(const LLAvatarName&)   { return "A resident"; }
    static std::string getString(std::string_view)      { return std::string(); }
};

class RlvHandler
{
public:
    bool hasBehaviour(ERlvBehaviour) const { return false; }
};

inline RlvHandler gRlvHandler;

#endif // RLV_SHIM_H
