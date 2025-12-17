# cml — C MANGA Plus Loader

`cml` is a C library + CLI to download chapters from mangaplus.shueisha.co.jp and export either raw images or `.cbz` archives.

## Provenance

This project was implemented with the OpenAI Codex CLI agent (model: GPT-5.2), based on the design and behavior of [hurlenko/mloader](https://github.com/hurlenko/mloader).

## License

GPL-3.0-only. See `LICENSE`.

## Build

Dependencies:

- `libcurl`
- `libzip`

Build:

```sh
make
./bin/cml --help
```

## Library API

Public header: `include/cml/cml.h`

### Linking

Build the library with `make` (produces `lib/libcml.a`), then link it into your program:

```sh
cc -Iinclude examples/download_chapter.c lib/libcml.a -lcurl -lzip -o download_chapter
```

### Types

#### `cml_status`

All library functions that return `cml_status` use these values:

- `CML_OK`: success
- `CML_ERR_INVALID`: invalid argument (e.g. NULL pointers, id == 0, unknown URL)
- `CML_ERR_OOM`: out of memory
- `CML_ERR_HTTP`: HTTP request failed (includes non-2xx responses and network errors)
- `CML_ERR_PROTO`: protobuf decode error / unexpected API payload
- `CML_ERR_IO`: filesystem error
- `CML_ERR_ZIP`: CBZ/ZIP error

Convert a status to a human-readable message with `cml_status_string()`.

#### `cml_output_format`

- `CML_OUTPUT_CBZ`: create CBZ archives (ZIP)
- `CML_OUTPUT_RAW`: write raw `.jpg` images to the filesystem

#### `cml_quality`

- `CML_QUALITY_SUPER_HIGH`
- `CML_QUALITY_HIGH`
- `CML_QUALITY_LOW`

#### Logging (`cml_log_fn`)

If set, the library calls:

```c
void (*cml_log_fn)(void *user, const cml_log_event *ev);
```

- `cml_log_event.level`: `CML_LOG_ERROR|WARN|INFO|DEBUG`
- `cml_log_event.message`: formatted message (valid only during the callback)

#### Progress (`cml_progress_fn`)

If set, the library calls:

```c
void (*cml_progress_fn)(void *user, const cml_progress_event *ev);
```

`cml_progress_event` fields (all strings are valid only during the callback):

- `stage`: `"metadata" | "images" | "export"`
- `title_name`, `title_author`: optional title metadata
- `title_done`, `title_total`: optional “which title out of total” counters (0 when unknown)
- `chapter_name`: optional chapter name from viewer metadata
- `chapter_no`, `chapter_title`: optional chapter number/title metadata
- `chapter_done`, `chapter_total`: optional “which chapter out of total” counters (0 when unknown)
- `done`, `total`: generic progress counters (when `total == 0`, total is unknown)

### Configuration: `cml_config`

Create a `cml_config` and pass it to `cml_create()`.

Important: `cml_create()` copies the config by value into the handle. Changing your original `cml_config` struct after calling `cml_create()` does not affect a running handle. To change settings, destroy the handle and create a new one with the updated config.

- `out_dir`: output directory (created as needed)
- `output`: `CML_OUTPUT_CBZ` or `CML_OUTPUT_RAW`
- `quality`: image quality (`cml_quality`)
- `split`: request server-side split for combined images (when supported)
- `min_chapter`: inclusive minimum chapter number filter (0 disables)
- `max_chapter`: inclusive maximum chapter number filter (0 means infinity)
- `last_only`: if true, download only the last chapter per title (after filtering)
- `include_chapter_title`: include chapter title in generated filenames
- `chapter_subdir`: for RAW output, save images in a per-chapter subdirectory
- `log_fn`: optional structured logging callback
- `progress_fn`: optional progress callback
- `user`: opaque pointer passed to callbacks

### Lifecycle

- `cml *cml_create(const cml_config *cfg);`
  - Returns NULL on invalid config or initialization failure.
- `void cml_destroy(cml *h);`

### Providing inputs

You can mix and match:

- `cml_status cml_add_chapter_id(cml *h, uint32_t chapter_id);` (`chapter_id` must be non-zero)
- `cml_status cml_add_title_id(cml *h, uint32_t title_id);` (`title_id` must be non-zero)
- `cml_status cml_add_url(cml *h, const char *url);`
  - Accepts MangaPlus URLs like:
    - `https://mangaplus.shueisha.co.jp/viewer/<chapter_id>`
    - `https://mangaplus.shueisha.co.jp/titles/<title_id>`
  - Returns `CML_ERR_INVALID` for unrecognized strings.

### Running

- `cml_status cml_run(cml *h);`

This performs all network requests and writes output to `out_dir`. Downloads are currently sequential. The downloader retries a small number of times for transient HTTP/network errors.

Output safety guarantees:

- CBZ output is written to a temporary `.cbz.part` and renamed to `.cbz` only on success.
- RAW images are written with an atomic `*.tmp` + rename strategy to minimize partial files.

## Example consumer program

See `examples/download_chapter.c` for a minimal consumer that downloads chapter `1013146`.

Public API: `include/cml/cml.h`
