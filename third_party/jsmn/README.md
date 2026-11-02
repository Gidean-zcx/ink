# jsmn (vendored)

- Source: https://github.com/zserge/jsmn — `jsmn.h`, commit in `UPSTREAM_COMMIT` (fetched 2026-11-02)
- License: MIT (see `LICENSE`)
- Used by: the portable system (`system/core/ink_json.c`) for hub messages. A zero-allocation
  tokenizer: parsing never touches the heap, which matters on the device (no heap churn, no libc
  malloc). The simulator and host tools keep using cJSON.
