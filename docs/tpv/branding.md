# Prism — Branding & Rebrand Surface

**Name:** Prism · **Channel string:** `Prism Release` (test builds: `Prism Test`,
project builds: `Prism Project <name>`).

TPV §5 compliance: "Prism" contains no LL trademark fragment ("Second"/"Life"/"SL"/"Linden")
and does not imply Linden Lab endorsement. The Second Life Eye-in-Hand logo may appear **only**
on the real SL login splash, never as Prism's own mark.

## Rebrand surface (M1 checklist)

| Area | Location | Action |
|---|---|---|
| Channel/version | CMake `VIEWER_CHANNEL` | `-DVIEWER_CHANNEL="Prism Release"` |
| Windows resources | `indra/newview/res/` (`.rc`, icons) | Prism icon set, product strings |
| App icons | `indra/newview/icons/` | Prism icon set |
| macOS bundle | `Info.plist`, bundle identifier | `com.prismviewer.*`, Prism display name |
| Login splash | login page assets | Prism splash (keep SL Eye-in-Hand only where required) |
| About floater | `indra/newview/skins/default/xui/en/floater_about.xml` | Prism name, credits, LGPL + upstream attribution |
| Update endpoint | viewer version-manager / settings | point at Prism's own updater or disable; **never** query LL's update service as the official viewer |
| Crash reporter | crash-report endpoint config | point at ours or disable |
| Defaults | `settings.xml` | Prism defaults (dark theme, etc.) |
| Login identity | channel/version sent at login | honestly identifies Prism (never spoof official) |

## Assets to produce
- App icon (Windows `.ico`, macOS `.icns`, Linux PNG set)
- Login splash / loading art
- Wordmark + monochrome mark for docs/site

Store source art and export scripts under `branding/` at the repo root.
