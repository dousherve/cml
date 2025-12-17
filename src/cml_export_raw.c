#include "cml_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *path_join2(const char *a, const char *b) {
  if (!a || !b) return NULL;
  size_t alen = strlen(a);
  size_t blen = strlen(b);
  int need = (alen > 0 && a[alen - 1] != '/');
  size_t n = alen + (need ? 1 : 0) + blen + 1;
  char *out = (char *)malloc(n);
  if (!out) return NULL;
  snprintf(out, n, need ? "%s/%s" : "%s%s", a, b);
  return out;
}

static cml_status exporter_common(cml *h, const cml_title *title, const cml_chapter *chapter, const cml_chapter *next,
                                  cml_exporter *e) {
  char *title_dir = NULL;
  char *prefix = NULL;
  char *suffix = NULL;
  char *chapter_dir = NULL;
  cml_status st =
      cml_build_names(title, chapter, next, h->cfg.include_chapter_title, &title_dir, &prefix, &suffix, &chapter_dir);
  if (st != CML_OK) return st;

  e->title_dir_name = title_dir;
  e->chapter_prefix = prefix;
  e->chapter_suffix = suffix;
  e->chapter_dir_name = chapter_dir;

  e->title_dir_path = path_join2(h->cfg.out_dir, e->title_dir_name);
  if (!e->title_dir_path) return CML_ERR_OOM;
  st = cml_mkdir_p(e->title_dir_path);
  return st;
}

static cml_status exporter_raw_init(cml *h, const cml_title *title, const cml_chapter *chapter, const cml_chapter *next,
                                    cml_exporter *e) {
  cml_status st = exporter_common(h, title, chapter, next, e);
  if (st != CML_OK) return st;
  if (h->cfg.chapter_subdir) {
    e->raw_dir_path = path_join2(e->title_dir_path, e->chapter_dir_name);
  } else {
    e->raw_dir_path = strdup(e->title_dir_path);
  }
  if (!e->raw_dir_path) return CML_ERR_OOM;
  return cml_mkdir_p(e->raw_dir_path);
}

cml_status cml_exporter_open(cml *h, const cml_title *title, const cml_chapter *chapter, const cml_chapter *next_chapter,
                             cml_exporter **out) {
  if (!h || !title || !chapter || !out) return CML_ERR_INVALID;
  *out = NULL;
  cml_exporter *e = (cml_exporter *)calloc(1, sizeof(*e));
  if (!e) return CML_ERR_OOM;
  e->fmt = h->cfg.output;

  cml_status st = CML_OK;
  if (e->fmt == CML_OUTPUT_RAW) {
    st = exporter_raw_init(h, title, chapter, next_chapter, e);
  } else {
    extern cml_status cml_export_cbz_init(cml *h, const cml_title *title, const cml_chapter *chapter,
                                          const cml_chapter *next, cml_exporter *e);
    st = cml_export_cbz_init(h, title, chapter, next_chapter, e);
  }

  if (st != CML_OK) {
    cml_exporter_close_destroy(e, false);
    return st;
  }
  *out = e;
  return CML_OK;
}

void cml_exporter_close_destroy(cml_exporter *e, bool success) {
  if (!e) return;
  if (e->zip && e->fmt == CML_OUTPUT_CBZ) {
    extern cml_status cml_export_cbz_finalize(cml_exporter *e);
    extern void cml_export_cbz_abort(cml_exporter *e);
    if (success) {
      (void)cml_export_cbz_finalize(e);
    } else {
      cml_export_cbz_abort(e);
    }
  } else if (e->zip) {
    zip_close(e->zip);
  }
  e->zip = NULL;
  free(e->title_dir_name);
  free(e->chapter_dir_name);
  free(e->chapter_prefix);
  free(e->chapter_suffix);
  free(e->title_dir_path);
  free(e->raw_dir_path);
  free(e->cbz_path);
  free(e);
}

int cml_exporter_skip_image(cml_exporter *e, int is_range, uint32_t start, uint32_t stop) {
  if (!e) return 1;
  if (e->skip_all) return 1;
  if (e->fmt == CML_OUTPUT_CBZ) return 0;
  char *filename = NULL;
  if (cml_format_page_filename(e->chapter_prefix, e->chapter_suffix, is_range, start, stop, "jpg", &filename) != CML_OK)
    return 0;
  char *path = path_join2(e->raw_dir_path, filename);
  free(filename);
  if (!path) return 0;
  int exists = cml_exists(path);
  free(path);
  return exists;
}

cml_status cml_exporter_add_image(cml_exporter *e, const uint8_t *data, size_t len, int is_range, uint32_t start,
                                  uint32_t stop) {
  if (!e || !data) return CML_ERR_INVALID;
  if (e->skip_all) return CML_OK;
  if (e->fmt == CML_OUTPUT_CBZ) {
    extern cml_status cml_export_cbz_add(cml_exporter *e, const uint8_t *data, size_t len, int is_range, uint32_t start,
                                         uint32_t stop);
    return cml_export_cbz_add(e, data, len, is_range, start, stop);
  }
  char *filename = NULL;
  cml_status st = cml_format_page_filename(e->chapter_prefix, e->chapter_suffix, is_range, start, stop, "jpg", &filename);
  if (st != CML_OK) return st;
  char *path = path_join2(e->raw_dir_path, filename);
  free(filename);
  if (!path) return CML_ERR_OOM;
  st = cml_write_file_atomic(path, data, len);
  free(path);
  return st;
}
