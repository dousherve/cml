#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CML_OK = 0,
  CML_ERR_INVALID = 1,
  CML_ERR_OOM = 2,
  CML_ERR_HTTP = 3,
  CML_ERR_PROTO = 4,
  CML_ERR_IO = 5,
  CML_ERR_ZIP = 6,
} cml_status;

typedef enum {
  CML_QUALITY_SUPER_HIGH = 0,
  CML_QUALITY_HIGH = 1,
  CML_QUALITY_LOW = 2,
} cml_quality;

typedef enum {
  CML_OUTPUT_CBZ = 0,
  CML_OUTPUT_RAW = 1,
} cml_output_format;

typedef enum {
  CML_LOG_ERROR = 0,
  CML_LOG_WARN = 1,
  CML_LOG_INFO = 2,
  CML_LOG_DEBUG = 3,
} cml_log_level;

typedef struct {
  cml_log_level level;
  const char *message;  // valid during callback
} cml_log_event;

typedef void (*cml_log_fn)(void *user, const cml_log_event *ev);

typedef struct {
  const char *stage;        // "metadata" | "images" | "export"
  const char *title_name;   // optional, valid during callback
  const char *title_author; // optional, valid during callback
  uint32_t title_done;      // optional, 0 when unknown
  uint32_t title_total;     // optional, 0 when unknown
  const char *chapter_name; // optional, valid during callback
  const char *chapter_no;   // optional, valid during callback (e.g. "60")
  const char *chapter_title; // optional, valid during callback
  uint32_t chapter_done;    // optional, 0 when unknown
  uint32_t chapter_total;   // optional, 0 when unknown
  uint32_t done;
  uint32_t total;  // 0 when unknown
} cml_progress_event;

typedef void (*cml_progress_fn)(void *user, const cml_progress_event *ev);

typedef struct {
  const char *out_dir;  // directory; created as needed
  cml_output_format output;
  cml_quality quality;
  bool split;

  uint32_t min_chapter;  // inclusive
  uint32_t max_chapter;  // inclusive, 0 means infinity
  bool last_only;

  bool include_chapter_title;
  bool chapter_subdir;

  cml_log_fn log_fn;
  cml_progress_fn progress_fn;
  void *user;
} cml_config;

typedef struct cml cml;

// Lifecycle
cml *cml_create(const cml_config *cfg);
void cml_destroy(cml *h);

// Inputs (you can mix and match)
cml_status cml_add_chapter_id(cml *h, uint32_t chapter_id);
cml_status cml_add_title_id(cml *h, uint32_t title_id);

// Accepts Mangaplus viewer/titles URLs; returns CML_ERR_INVALID on unrecognized strings.
cml_status cml_add_url(cml *h, const char *url);

// Main entrypoint
cml_status cml_run(cml *h);

// Utilities
const char *cml_status_string(cml_status st);

#ifdef __cplusplus
}  // extern "C"
#endif
