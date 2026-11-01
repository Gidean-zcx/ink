# Batch 37 worker validation

Checked on: 2026-10-27

Scope: local mechanical validation only. No network access and no edits under `docs/` were made by this validation task.

## Commands and exit status

1. `python3 work/research/interaction-depth/check_docs.py`
   - Exit status: `0`
   - Result: 233 Markdown files, 1,757 local links, 65 products, 65 partial cases, 920 flows (`D2=827`, `D1=77`, `D4=16`), 1,577 evidence records, 1,409 distinct source URLs, 215 static-visual evidence records, 19 operated-browser evidence records, `errors=[]`.

2. `git diff --check`
   - Exit status: `0`
   - Output: none.

3. Local Python comparison of:
   - `work/research/batch37-integration/changes.json`
   - `work/research/batch37-integration/before/docs/research/interaction-library/coverage.csv`
   - current `docs/research/interaction-library/coverage.csv`
   - the corresponding before/current `evidence.jsonl`
   - the before/current Mudita and Khan inventory CSV files, keyed by their actual `inventory_id` field
   - `docs/requirements.md`
   - Exit status: `0`.

4. The comparison result was written temporarily to `/tmp/batch37-worker-validation.json` for command evidence only; it is outside the project and is not a formal artifact.

## Main-index preservation and declared changes

- Before main coverage: 906 flow IDs; current: 920.
- Every old flow ID remains present.
- Added flow IDs: exactly 14, and the set exactly equals `changes.json.new_flow_ids`.
- Changed pre-existing flows: exactly 10, and their IDs, complete `before` records, and complete `after` records exactly equal `changes.json.updated_flows`.
- Before main evidence: 1,518 evidence IDs; current: 1,577.
- Every old evidence ID remains present.
- Added evidence IDs: exactly 59, and the set exactly equals `changes.json.new_evidence_ids`.
- Changed pre-existing evidence: exactly 1, and its ID, complete `before` record, and complete `after` record exactly equal `changes.json.updated_evidence`.
- No undeclared old flow or evidence record change was found by whole-record comparison.

## Inventory checks

- Mudita inventory remains 111 data rows before and after, with the same 111 `inventory_id` values.
- Its 46 former `index_only` rows are all now `body_read`.
- Khan inventory remains 130 data rows before and after, with the same 130 `inventory_id` values.
- Exactly 10 former Khan `index_only` rows changed to `body_read_batch37`: `KLC-I011`, `KLC-I012`, `KLC-I013`, `KLC-I014`, `KLC-I019`, `KLC-I040`, `KLC-I052`, `KLC-I109`, `KLC-I110`, `KLC-I112`.
- No other Khan `body_status` changed.
- The added `upstream_title` column is non-empty on exactly 112 rows.
- `KLC-I025` and `KLC-I026` retain their previous `flow_ids` and `evidence_ids` exactly.

## Requirements invariant

- `docs/requirements.md` SHA-256: `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`.
- This exactly matches the required invariant.

## Errors, unknowns, and state change

- Errors: none.
- Conflicts: none.
- Unknowns within the requested mechanical checks: none.
- Project state changed only by creating this `worker-validation.md`; no formal source, coverage, evidence, inventory, requirements, or other `docs/` file was edited by this task.
