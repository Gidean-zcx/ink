# stb_image_write (vendored)

- Source: https://github.com/nothings/stb — `stb_image_write.h` v1.16
- Commit: see `UPSTREAM_COMMIT` (fetched 2026-11-02)
- License: public domain (Unlicense) or MIT, at your choice — see the end of the header.
- Used by: the simulator only (`ports/sim`), to write PNG screenshots of the simulated panel. Never linked into device firmware.

## stb_image

- Reused source: LVGL **v9.5.0**, `src/libs/gltf/stb_image/stb_image.h`, **v2.30** (2024-05-31). Copied from the pinned build dependency on 2026-11-08; no new download.
- Original SHA-256: `d81d95287ae188e472fe66ddcad8635041e8f29d51a799cc77c65c6e94400f1c`, 303477 bytes.
- License: **MIT**, preserved at the end of `stb_image.h`.
- Local patch: the PNG inflate entry point's `stbi__zbuf` and its Huffman workspace move from stack to `STBI_MALLOC`/`STBI_FREE`; see the marked header and task report for the final hash.
- Reason for a local copy: the dependency directory is read-only; its unmodified PNG decoder exceeds the device task stack. LVGL's LodePNG uses UI globals/draw buffers, and the current JPEG HAL accepts only a simulation format.
- Used by: portable image helpers, compiled for PNG/JPEG only, without host file IO, SIMD or HDR; allocation hooks require PSRAM. Other stb APIs are private to this implementation and are not part of the system SDK.
