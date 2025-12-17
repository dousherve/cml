# cml — C MANGA Plus Loader

`cml` is a C library + CLI to download chapters from mangaplus.shueisha.co.jp and export either raw images or `.cbz` archives.

## Provenance

This project was implemented with the OpenAI Codex CLI agent (model: GPT-5.2), based on the design and behavior of `hurlenko/mloader` (https://github.com/hurlenko/mloader).

## Build

Dependencies:

- `libcurl`
- `libzip`

Build:

```sh
make
./bin/cml --help
```

## Library

Public API: `include/cml/cml.h`
