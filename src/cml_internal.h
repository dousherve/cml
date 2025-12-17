#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <curl/curl.h>
#include <zip.h>

#include "cml/cml.h"

typedef struct {
  uint8_t *data;
  size_t len;
} cml_bytes;

typedef struct {
  uint32_t *items;
  size_t len;
  size_t cap;
} cml_u32_vec;

typedef struct {
  uint32_t chapter_id;
  char *name;       // Chapter.name
  char *sub_title;  // Chapter.sub_title
} cml_chapter;

typedef struct {
  uint32_t title_id;
  char *name;
  char *author;
  int32_t language;
} cml_title;

typedef struct {
  char *image_url;
  char *encryption_key;
  int32_t type;
} cml_manga_page;

typedef struct {
  cml_chapter current_chapter;
  cml_chapter next_chapter;
  bool has_next_chapter;
} cml_last_page;

typedef struct {
  bool has_manga_page;
  cml_manga_page manga_page;
  bool has_last_page;
  cml_last_page last_page;
} cml_page;

typedef struct {
  cml_page *pages;
  size_t pages_len;
  cml_chapter *chapters;
  size_t chapters_len;
  uint32_t chapter_id;
  uint32_t title_id;
  char *chapter_name;
} cml_manga_viewer;

typedef struct {
  cml_chapter *first;
  size_t first_len;
  cml_chapter *last;
  size_t last_len;
} cml_chapter_group;

typedef struct {
  cml_title title;
  cml_chapter_group *groups;
  size_t groups_len;
} cml_title_detail;

typedef struct cml_exporter cml_exporter;

struct cml_exporter {
  cml_output_format fmt;
  char *title_dir_name;
  char *chapter_dir_name;
  char *chapter_prefix;
  char *chapter_suffix;

  char *title_dir_path;  // <out>/<title>
  char *raw_dir_path;    // where images go when RAW
  char *cbz_path;        // path to .cbz when CBZ
  zip_t *zip;
  bool skip_all;
};

struct cml {
  cml_config cfg;
  CURL *curl;
  cml_u32_vec chapter_ids;
  cml_u32_vec title_ids;
};

// Logging/progress (no-ops if callbacks not set)
void cml_log(cml *h, cml_log_level level, const char *fmt, ...);
void cml_progress(cml *h, const cml_progress_event *ev);

// ids
int cml_u32_push(cml_u32_vec *v, uint32_t x);
int cml_u32_sort_dedupe(cml_u32_vec *v);
void cml_u32_free(cml_u32_vec *v);

// url
int cml_url_extract_viewer_id(const char *s, uint32_t *out);
int cml_url_extract_titles_id(const char *s, uint32_t *out);

// http
cml_status cml_http_get(cml *h, const char *url, cml_bytes *out);
void cml_bytes_free(cml_bytes *b);

// api
cml_status cml_api_get_manga_viewer(cml *h, uint32_t chapter_id, cml_manga_viewer *out);
cml_status cml_api_get_title_detail(cml *h, uint32_t title_id, cml_title_detail *out);

// proto
cml_status cml_proto_parse_manga_viewer(const uint8_t *buf, size_t len, cml_manga_viewer *out);
cml_status cml_proto_parse_title_detail(const uint8_t *buf, size_t len, cml_title_detail *out);
void cml_proto_free_manga_viewer(cml_manga_viewer *v);
void cml_proto_free_title_detail(cml_title_detail *d);

// crypto
cml_status cml_decrypt_xor_hex(uint8_t *data, size_t data_len, const char *hex_key);

// fs
cml_status cml_mkdir_p(const char *path);
int cml_exists(const char *path);
cml_status cml_write_file_atomic(const char *path, const uint8_t *data, size_t len);
cml_status cml_rename_overwrite(const char *src, const char *dst);

// naming
cml_status cml_build_names(const cml_title *title, const cml_chapter *chapter, const cml_chapter *next_chapter,
                           bool include_chapter_title, char **out_title_dir, char **out_chapter_prefix,
                           char **out_chapter_suffix, char **out_chapter_dir);
cml_status cml_format_page_filename(const char *chapter_prefix, const char *chapter_suffix, int is_range,
                                    uint32_t start, uint32_t stop, const char *ext, char **out);
char *cml_escape_component(const char *s);
char *cml_titlecase_ascii(const char *s);
int cml_chapter_name_to_int(const char *s, int *out);
int cml_is_oneshot(const char *chapter_name, const char *chapter_subtitle);

// exporters
cml_status cml_exporter_open(cml *h, const cml_title *title, const cml_chapter *chapter, const cml_chapter *next_chapter,
                             cml_exporter **out);
void cml_exporter_close_destroy(cml_exporter *e, bool success);
int cml_exporter_skip_image(cml_exporter *e, int is_range, uint32_t start, uint32_t stop);
cml_status cml_exporter_add_image(cml_exporter *e, const uint8_t *data, size_t len, int is_range, uint32_t start,
                                  uint32_t stop);

// loader
cml_status cml_loader_run(cml *h);
