# Batch50 completed — overall goal still active

2026-10-27. Research/documentation only. No new product UI/device execution or implementation. Requirements SHA unchanged: e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087.

- Frozen totals: 65 partial groups; 967F = 869D2 + 78D1 + 20D4; 1713E / 1542 distinct URLs; 229 static E; 21 browser E; 122 RP.
- Docs check: 269 Markdown, 2262 local links, errors []; root verification passed. Formal diff 8 new / 21 modified / 0 deleted. See final-manifest.json and root-final-verification.json.
- Latest brief: docs/briefs/050-interaction-depth-and-reuse.md; root/library READMEs point there; all 50 briefs indexed.
- integrate_once.py and documents_once.py APPLIED ONCE. Do not rerun. verify_root.py is safe but counts are batch50-specific.

## Substantive additions

- M042: two official Android English help bodies: notes 15230597 (E-GENR-01), tasks/reminders 15230285 (E-GENR-02). Captured web-tool text, not raw HTML; source-notes/audit in artifacts/gemini-notes-reminders. F12 notes/list and F13 to-dos are new D2; F02/F10 extended. 13 total M042 flows, all D2.
- Last-accessed default app vs @ explicit target; per-app action/content limitations; post-action Undo is not pre-write approval; Live background exceptions. No actual write/undo/notification test. Google Tasks to-do objects are not the separate Tasks agent workflow.
- Calendar 15305236 returned HTTP429; stop, no alternate. Workspace15229592 not accessible; no body. Earlier Tasks17094507/Skills17094296/Schedules17094710 searches remain discovery only; do not upgrade or merge with to-do task.
- Gemini original inventory 129→133 rows, still85 connector entries; 13 existing rows corrected/updated (3 previously-read Phone/Messages/Device assistance, 8 OEM notes, Reminder, Workspace partial child-help only), 4 documented control rows added. Directory evidence field remains E-GECD-01; new supporting E is in gap and referenced F.
- M060 QUADERNO (not M061 Huawei): new E-QGNT-01 for previously saved Gen.2 PDF B6FK5131.pdf, FMVDP41/FMVDP51, B6FK-5131-08 Z0/Z0-02 issued2025-04. Ten full pages read:1,27,51–57,124. Model fields only checked p120, not full body read. Existing PDF SHA7bedbd0094f1b4471c7fe2049dd9d379bb09ac4485765d1aafb267dfc474d870,124pages.
- F11/F12 add version-specific Gen.2 branches; F17 calibration added. Total17 M060 D2. Calibration stroke appears only after pen lift, adjustment/verification loop/save; ink persists vs single-use clipboard vs page-local undo; grayscale display still contains color data; other-device annotation edit restriction. No image/device operation.
- New Gen.2 inventory197 extracted TOC nodes, most unreviewed. Start-page read ≠ whole section; no automatic F completion.

## Reuse fixes

- reference-points.csv expanded15→22 columns by merge_batches.py. Original15 remain; derived7: task_depths, task_hands_on, source_types, access_modes, evidence_hands_on, operation_methods, operation_audit_paths. First6 are ID-keyed JSON strings; audit paths semicolon-separated or none_for_cited_evidence.
- F depth does not make its every E an operated source. Actual E audit remains separate from image artifact_path. 21 audit-path associations in RP are not independent operation counts.
- data-dictionary fixed false four-value evidence_basis enumeration; free text stays free text. case-index generated label now task research depth with in-page D1–D4 explanation.
- Eight new RP: M04201–04, M06001–04. All are documentary, no UI success claim.

## Helper audit and root correction

worker /root/batch50_quaderno_inventory completed two read-only reports in this directory, no formal edits/network. Root corrected material page-count scope: work/quaderno-controls-deepening/read-pages.json body_read_for_this_batch lists76 pages, not global union; its58-page complement includes previously observed Gen.3C p68/69. Do NOT report all58 as globally unread. Formal notes/case/backlog corrected; root-corrections.json records this. Do not reuse ambiguous inventory-count conclusion without root verification.

## Next bounded research

- Gen.2 p85–92 transfer/backup/sync; p35–39 deletion/recycle; p110–113 failures. Other-body read state currently absent; root selected10-page list above remains authoritative for batch50.
- Gen.3C connect/file lifecycle candidates p38–45,88–94,103–105; merge all historical captures before declaring global unread count. Existing Gen.3C PDF SHA d7248dfc9cd602139f4a5dddf03c33809b02b00d9f13601cf73f17ad1b8ac5f4,134pages.
- Continue broad per-product gaps; no goal-completion proof yet. All65 groups remain partial; no genuine block.
- Disk remains tight (~18MB at start; no large downloads/install/cache purge this batch). Previous batch49 narrowly removed only regenerable Chromium Cache, preserving other profile files; do not touch outside workspace.
