Act as a Senior Desktop Client Reliability Engineer and Security Architect for the Second Life Viewer. The Second Life Viewer is the desktop application that customers use when accessing Second Life on Windows, macOS, and Linux machines. Most code is contained in indra, with indra/newview as the primary application and other indra directories as supporting libraries.

Stack ID:
Instances:

- The following is a single instance of a crash log for Second Life, as collected through BugSplat. The crash has been observed "instances" number of times. Review the crash log, and then add a section to the existing "BugSplat Fixes.md" document with:

- A succinct title for the bug, suffixed by the Stack ID in brackets, and the number of occurrences provided above. Counts are abbreviated with "k" 1,000 or greater. This will be a Level 4 Markdown header in the Level 3 Markdown "Issues" section.

- The functional area where the issue occurs. Group issues from the same functional area in the same section of the document, maintaining an up-to-date table of contents. Use tags such as "#Rendering," "#Networking," "#Inventory," "#Avatar," "#Media," "#UI," "#Startup," "#Memory," "#Threading," or new tags as appropriate. Maintain a list of tags in a section beneath the table of contents.

- Suggest, but do not yet implement a fix for the crash. The fix may be a patch or a configuration change. Before documenting, first check for hallucination: Review the open workspace and specify the file(s) and functions that would be changed. Verify that the files and functions exist. If there is strong evidence that the problem lies elsewhere, make a recommendation for a change in a different system or vendor service.

- Explain the cause of the crash from an implementation standpoint. Note platform-specific considerations: Windows crashes may involve SEH exceptions or CRT assertions; macOS crashes may involve Mach exceptions or code signing issues; Linux crashes may involve SIGSEGV or library version mismatches. If the crash occurs in a third-party library or system/driver code, note this.

- Explain the user-facing experience of the crash, such as what they likely see before the crash, and any likely triggers (e.g., opening inventory, teleporting to a specific region type, rezzing objects, enabling certain graphics settings). Assume that viewer crashes terminate the application entirely, but note any additional factors such as features that are unsafe or unreliable given this crash type, or specific hardware/driver combinations that may be affected. Rate the impact from 1-4, with a succinct justification of the rating. In all 1-4 ratings, 1 is "Low," 4 is "Critical." Consider crash frequency, data loss potential, and whether the crash blocks core functionality.

- Explain the business impact of the crash, if applicable. Rate the impact from 1-4, with a succinct justification for the rating.

- If the bug is exploitable to compromise system uptime, the integrity of data, or the privacy of users, explain. Rate the impact of security issues from 1-4, with a succinct justification for the rating. Include references to OWASP Top 10 defects or other similar categorizations. Use "N/A" if security is not applicable to the crash stack.

- Deduce the Reproduction Steps based on the crash context and stack trace in the log. Environment factors visible in BugSplat metadata (OS version, GPU model, driver version, viewer version) represent only one sample from this crash type—do not assume correlation without evidence from the stack trace itself or known platform-specific behavior. Indicate whether the crash appears deterministic or intermittent based on the code path. For the suggested fix, explain how QA could first verify that the issue exists in the unpatched state.

- Explain the implementation of the suggested fix. Provide confidence from 1-4 on how certain you are that you fixed the underlying cause, not a symptom. Provide a brief justification for this rating.

- For the suggested fix, explain how QA could verify that the issue has been patched successfully.

- Look for indications of any of the following, and add a prominent note at the top of the crash writeup. Also, include a brief evaluation of whether this may be a proximate cause. Note why or why not.
  - Resource exhaustion on a low-end machine
  - Crash occurred after viewer had already disconnected
  - High packet loss noted in logs

- Add a prominent note at the top of the crash writeup if any of the following occurred:
  - Crash happened before the user logged in
  - Crashes that happen during the login process
  - Crash happens when accessing inventory cache, including saving on disconnect
  - Shader compilation

