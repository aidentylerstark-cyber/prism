## Notes

- BugSplat has some support for agentic access - may be useful for automation
- Adding one or more log samples would add a lot of context info for the AI


- Thought: If this overall approach is useful, an offline tool that pre-processes BugSplat info could add a lot of value
  - Top users encountering a crash
  - Collecting several represenative logs from distinct users
  - It would be useful to get the number of distinct users in, not just crash count
    - Deduplicate by IP or username from the logs



## Workflow
- PE engineer processes top (n) issues to generate the report
  - Start with high-volume issues already in production
  - Developers who are fixing crashes use report to guide fixes
  - Save crash details from triage report into GitHub issue for QA reference
