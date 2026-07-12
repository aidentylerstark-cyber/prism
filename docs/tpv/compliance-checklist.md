# Prism — TPV Policy Compliance Checklist

Living document. **Every PR is reviewed against this list.** Sources: the Second Life
[Third Party Viewer Policy](https://secondlife.com/corporate/third-party-viewers), the
[TPV Directory wiki](https://wiki.secondlife.com/wiki/Third_Party_Viewer_Directory), and the
[Directory FAQ](https://wiki.secondlife.com/wiki/Linden_Lab_Official:Third_Party_Policy_and_Viewer_Directory_FAQ).

## Hard constraints

- [ ] **Naming (§5):** name/branding contains no LL trademark or confusingly-similar fragment
      — no "Second", "Life", "SL", "Linden". Eye-in-Hand logo appears only on the actual SL
      login splash. No implied LL endorsement. → *"Prism" complies.*
- [ ] **Honest identity (§2):** channel/version string sent at login honestly identifies Prism;
      never spoof the official viewer or another TPV.
- [ ] **No private-data exposure (§2.a):** no feature reveals other residents' system/network
      info (IP/MAC), or presence/location beyond what the simulator already broadcasts to every
      viewer (e.g. CoarseLocationUpdate). Radar/area-search stay within sim-provided data only.
- [ ] **No permission/DRM circumvention (§2.b):** never strip creator metadata or bypass
      content permissions. Export/backup only for content the user created with full perms.
- [ ] **No grid harm (§2):** respect throttles; no message flooding; graceful reconnect;
      nothing that degrades the grid or other residents' experience.
- [ ] **Shared experience:** do not misrepresent other creators' content (e.g. don't
      default-disable their intended rendering). Opt-in post-processing of legitimately-received
      data is fine.

## Distribution & transparency

- [ ] **LGPL source availability (§3.b):** LGPL-covered source is public. Public repo satisfies this.
- [ ] **Public development (§6.a.iv):** publicly viewable source repo with public commits +
      a public contact channel.
- [ ] **Published privacy policy (§4.b):** describes all user data collected/stored/used; no
      user data required to install/uninstall; no sharing collected data without explicit consent.
      → see [privacy-policy.md](privacy-policy.md).
- [ ] **Third-party library licenses:** audit the LGPL/GPL/Apache/BSD mix declared in `autobuild.xml`.

## Directory registration (M4)

- [ ] Accounts in good standing + payment-info-on-file or age verification.
- [ ] File a "New Application" issue at
      [`secondlife/third-party-viewers`](https://github.com/secondlife/third-party-viewers).
- [ ] Directory listing obtained before public launch (self-certification model).

## Features needing dedicated review when reached

- [ ] **Poser / pose sync** (M3): manipulating *own* avatar is fine (Black Dragon ships it and is
      directory-listed); syncing poses onto others needs care re: shared-experience norms.
- [ ] **LSL client bridge** (M2 later): an in-world scripted attachment à la Firestorm — review
      against §2 before shipping.
