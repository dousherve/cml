# cml (C MANGA Plus Loader) — Agent Notes

This file is intended to give future assistants quick, accurate context for continuing work on this `cml/` repo without re-analyzing the whole codebase.

## What this is

`cml` is a C library + CLI to download chapters from `mangaplus.shueisha.co.jp` / the MangaPlus API and export either:

- `.cbz` archives (ZIP), or
- raw `.jpg` images

Target: Linux + macOS. Toolchain: clang + Makefile.

## Build / Run

Dependencies:

- `libcurl`
- `libzip`
- `pkg-config` (optional; Makefile falls back to `-lcurl -lzip`)

Commands:

- Build: `make`
- Help: `./bin/cml --help`
- Example: `./bin/cml --title 100644 --last -o cml_test_downloads`

Environment variables used by the CLI:

- `CML_OUT_DIR`
- `CML_RAW` (truthy => raw output)
- `CML_QUALITY` (`super_high|high|low`)

## Public library API

Header: `include/cml/cml.h`

Key concepts:

- `cml_config`: all runtime settings + optional callbacks
  - `log_fn`: structured log callback (`cml_log_event`)
  - `progress_fn`: progress callback (`cml_progress_event`)
- Inputs:
  - `cml_add_chapter_id`, `cml_add_title_id`, `cml_add_url`
- Run:
  - `cml_run`

The CLI (`src/cml_cli.c`) parses args first, then instantiates the library handle once the final config is known (important: `-o/--out` must work regardless of option ordering).

## Internal architecture (files)

- `src/cml.c`: top-level handle, config validation, callbacks, add inputs, run entrypoint
- `src/cml_http.c`: GET via libcurl + conservative retries/backoff
- `src/cml_api.c`: MangaPlus endpoints:
  - `/api/manga_viewer`
  - `/api/title_detailV3`
- `src/cml_proto.c`: minimal protobuf wire decoder (no codegen) for fields needed by loader/export
- `src/cml_crypto.c`: XOR decrypt using hex key (`encryption_key`)
- `src/cml_loader.c`: normalization (mix title+chapter inputs), filtering (`begin/end/last`), download loop
- `src/cml_naming.c`: path sanitization + naming scheme (kept compatible with `mloader` patterns)
- `src/cml_export_raw.c`: raw export + atomic writes
- `src/cml_export_cbz.c`: CBZ export using a `.cbz.part` temp + rename on success
- `src/cml_fs.c`: mkdirp, atomic write, rename-overwrite helpers
- `src/cml_ids.c`: u32 vector helpers
- `src/cml_url.c`: extracts IDs from `/viewer/<id>` and `/titles/<id>`

## Important behaviors / guarantees

- CBZ output is written to a temporary `.cbz.part` and only renamed to `.cbz` on successful completion.
- On failure while writing CBZ, the `.part` is discarded/removed (best-effort).
- Raw image output uses atomic write via `*.tmp` + rename, so partial files are minimized.
- HTTP retries: limited retries for transient errors (429/5xx and common network errors).

## Current limitations / follow-ups

- Protobuf decoding is “minimal-field” and may break if MangaPlus schema changes; adding a small compatibility test (decode a known captured response) would help.
- Progress callback is intentionally simple; could add per-title/chapter events and ETA.
- Concurrency is currently sequential; parallel downloads could be added but must consider throttling.
