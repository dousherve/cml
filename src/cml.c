#include "cml/cml.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "cml_internal.h"

const char *cml_status_string(cml_status st) {
  switch (st) {
    case CML_OK:
      return "ok";
    case CML_ERR_INVALID:
      return "invalid argument";
    case CML_ERR_OOM:
      return "out of memory";
    case CML_ERR_HTTP:
      return "http error";
    case CML_ERR_PROTO:
      return "protobuf decode error";
    case CML_ERR_IO:
      return "io error";
    case CML_ERR_ZIP:
      return "zip error";
    default:
      return "unknown error";
  }
}

void cml_log(cml *h, cml_log_level level, const char *fmt, ...) {
  if (!h || !fmt) return;
  if (!h->cfg.log_fn) return;

  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  cml_log_event ev = {.level = level, .message = buf};
  h->cfg.log_fn(h->cfg.user, &ev);
}

void cml_progress(cml *h, const cml_progress_event *ev) {
  if (!h || !ev) return;
  if (!h->cfg.progress_fn) return;
  h->cfg.progress_fn(h->cfg.user, ev);
}

static const char *quality_str(cml_quality q) {
  switch (q) {
    case CML_QUALITY_SUPER_HIGH:
      return "super_high";
    case CML_QUALITY_HIGH:
      return "high";
    case CML_QUALITY_LOW:
      return "low";
    default:
      return "super_high";
  }
}

static int cfg_valid(const cml_config *cfg) {
  if (!cfg) return 0;
  if (!cfg->out_dir || !*cfg->out_dir) return 0;
  return 1;
}

cml *cml_create(const cml_config *cfg) {
  if (!cfg_valid(cfg)) return NULL;

  static int curl_inited = 0;
  if (!curl_inited) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return NULL;
    curl_inited = 1;
  }

  cml *h = (cml *)calloc(1, sizeof(*h));
  if (!h) return NULL;
  h->cfg = *cfg;

  h->curl = curl_easy_init();
  if (!h->curl) {
    free(h);
    return NULL;
  }

  if (h->cfg.max_chapter && h->cfg.max_chapter < h->cfg.min_chapter) {
    cml_destroy(h);
    return NULL;
  }

  cml_log(h, CML_LOG_DEBUG, "quality=%s split=%d output=%d", quality_str(h->cfg.quality), (int)h->cfg.split,
          (int)h->cfg.output);
  return h;
}

void cml_destroy(cml *h) {
  if (!h) return;
  if (h->curl) curl_easy_cleanup(h->curl);
  cml_u32_free(&h->chapter_ids);
  cml_u32_free(&h->title_ids);
  free(h);
}

cml_status cml_add_chapter_id(cml *h, uint32_t chapter_id) {
  if (!h || chapter_id == 0) return CML_ERR_INVALID;
  if (cml_u32_push(&h->chapter_ids, chapter_id) != 0) return CML_ERR_OOM;
  return CML_OK;
}

cml_status cml_add_title_id(cml *h, uint32_t title_id) {
  if (!h || title_id == 0) return CML_ERR_INVALID;
  if (cml_u32_push(&h->title_ids, title_id) != 0) return CML_ERR_OOM;
  return CML_OK;
}

cml_status cml_add_url(cml *h, const char *url) {
  if (!h || !url || !*url) return CML_ERR_INVALID;
  uint32_t id = 0;
  if (cml_url_extract_viewer_id(url, &id)) return cml_add_chapter_id(h, id);
  if (cml_url_extract_titles_id(url, &id)) return cml_add_title_id(h, id);
  return CML_ERR_INVALID;
}

cml_status cml_run(cml *h) {
  if (!h) return CML_ERR_INVALID;
  if (h->chapter_ids.len == 0 && h->title_ids.len == 0) return CML_ERR_INVALID;
  if (cml_u32_sort_dedupe(&h->chapter_ids) != 0) return CML_ERR_OOM;
  if (cml_u32_sort_dedupe(&h->title_ids) != 0) return CML_ERR_OOM;
  return cml_loader_run(h);
}

