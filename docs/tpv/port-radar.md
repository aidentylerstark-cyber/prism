# Port Spec — Radar (Firestorm → Prism)

Execution spec for porting Firestorm's nearby-avatar radar into Prism. All claims verified
against real source in `/home/runner/reference/firestorm` and our base tree (LL 26.2).
Firestorm radar is LGPL-2.1 (Ansariel Hiller headers) — **keep the headers, add attribution**.

## Source inventory (indra/newview/ in Firestorm)
| File | Role |
|---|---|
| `fsradar.{cpp,h}` | Core model. `FSRadar : LLSingleton`; periodic sweep via `LLEventTimer`; pulls avatars from `LLWorld::getAvatars`; maintains `FSRadarEntry` list; chat/draw/sim range enter-leave alerts; publishes rows as `vector<LLSD>`. |
| `fsradarentry.{cpp,h}` | Per-avatar record: name (LLAvatarNameCache), age/flags/notes (LLAvatarPropertiesProcessor via AgentProfile cap), RLV `@shownames` anonymization. |
| `fspanelradar.{cpp,h}` | UI panel: `FSRadarListCtrl`, embedded `LLNetMap`, filter editor, options menu. |
| `fsfloaterradar.{cpp,h}` | Trivial `LLFloater` shell (standalone mode). |
| `fsradarlistctrl.{cpp,h}` | `FSRadarListCtrl : FSScrollListCtrl`; XUI widget tag `radar_list`. |
| `fsradarmenu.{cpp,h}` | Context menus (single + multiselect), mostly `LLAvatarActions::*`. |
| `fsscrolllistctrl.{cpp,h}` | **7th pair, must port too** — base class; deps only on base viewer. |

XUI: `panel_fs_radar.xml`, `floater_fs_radar.xml`, `menu_fs_radar.xml`, `menu_fs_radar_multiselect.xml`, `menu_fs_radar_options.xml`.

## Dependencies already in our base (verified present — port maps cleanly)
`LLWorld::getAvatars` (llworld.h:163), `LLAvatarPropertiesProcessor` incl. `hide_age`/`notes`
(llavatarpropertiesprocessor.h), `LLNotificationManager::onChat`, `LLListContextMenu`, `LLNetMap`
(+`setSelected`), `LLSpeakerMgr`, `LLTracker`, `LLMuteList`, `LLAvatarNameCache`, `LLVOAvatar`
typing/sit state, and most `LLAvatarActions::*` menu actions.

## Firestorm-only deps (MISSING — decision per item)
- **Port small:** `fsscrolllistctrl` (base class, ~280 lines); minimal `fscommon` subset (`isLinden`, `format_string`, `report_to_nearby_chat`, `isFilterEditorKeyCombo`).
- **Shim:** RLVa (`rlvshim.h`: `canShowNearbyAgents()→true`, `hasBehaviour()→false`) — ~15 call sites; swap for real RLVa if/when ported.
- **Strip for MVP:** `fskeywords`, `lggcontactsets` (contact-set colorization), `fsassetblacklist` (derender), `fslslbridge` z-offset block, `fsavatarrenderpersistence`, `fsfloateravataralign`, FS `LLNetMap` mark-color extensions, FS estate/land/report/derender menu items.
- **Replace with SL constants:** `lfsimfeaturehandler` say/shout ranges → `SAY=20m, SHOUT=100m`.
- **Adapt:** voice-level column → base `LLVoiceClient::getCurrentPower()` thresholds (or omit for MVP); `getUserNameForDisplay` → base `getUserName`.

## Integration points in base tree
1. `llviewerfloaterreg.cpp` — register `fs_radar` floater.
2. `indra/newview/CMakeLists.txt` — add 7 `.cpp` + 7 `.h`.
3. `app_settings/settings.xml` — radar keys (`RadarNameFormat`, `RadarAvatarAgeAlert*`, `LimitRadarByRange`, `NearMeRange`, `RadarReport*Range*`, `Radar*ChannelAlert`, `RadarAlertChannel` default `-777777777`, `ShowRadarMinimap`, column config).
4. `app_settings/commands.xml` + toolbar icon + `Command_Radar_*` strings.
5. `strings.xml` — alert strings (`entering/leaving_chat_range`, `entering/leaving_draw_distance`, `entering/leaving_region`, `avatar_age_alert`, etc.).
6. `textures.xml` — `Radar_VoicePTT_*` aliases (point at existing base PNGs; none new needed).
7. Copy the 5 XUI files into `skins/default/xui/en/` (packaging auto-globs).
8. Startup: instantiate `FSRadar::instance()` in `llstartup.cpp`; shutdown guard in `llappviewer.cpp`.

## TPV Policy compliance (§2.a) — VERDICT: compliant
Every data source is sim-broadcast or the user's own data: avatar IDs/positions from
`LLWorld::getAvatars` (CoarseLocationUpdate + in-view ObjectUpdate — same as the official
minimap); names from People API; age/flags/notes via AgentProfile cap **with `hide_age`
respected** and notes guarded by `agent_id == gAgentID`; voice/typing/sit from broadcast
indicators; enter/leave script alerts are self-addressed `ScriptDialogReply` (throttled, 6/msg).
**Discipline when porting:** keep the `hide_age` branch and `agent_id == gAgentID` guards
verbatim; add NO remote logging/telemetry of enter-leave events (§2.c).

## Ordered plan
- **Phase 0 (~1 day):** port `fsscrolllistctrl`, minimal `fscommon`, `rlvshim.h`.
- **Phase 1 MVP (~2-3 days):** port `fsradarentry`, `fsradar` (with strips), `fsradarlistctrl`,
  `fspanelradar`, `fsfloaterradar`, `fsradarmenu` (pruned); copy+prune XUI; wire integration
  points; smoke test (opens, lists nearby, filter, double-click zoom, context menu, no crash on
  region-cross/teleport/logout).
- **Phase 2:** alert sounds, script-channel alerts, voice column, age alert, translations.
- **Phase 3 (deferred until their subsystems land):** LSL-bridge z-offsets, contact-set colors,
  real RLVa, estate/land moderation menu, People-panel embedding.

**Effort:** Phase 0+1 ≈ 4–5 focused days. The radar core is clean; cost lives in the stripped FS
subsystems, none of which the MVP needs.
