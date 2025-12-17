#include "cml_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static char *path_with_ext(const char *path, const char *ext_with_dot) {
  if (!path || !ext_with_dot) return NULL;
  size_t n = strlen(path) + strlen(ext_with_dot) + 1;
  char *out = (char *)malloc(n);
  if (!out) return NULL;
  snprintf(out, n, "%s%s", path, ext_with_dot);
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

cml_status cml_export_cbz_init(cml *h, const cml_title *title, const cml_chapter *chapter, const cml_chapter *next,
                               cml_exporter *e) {
  cml_status st = exporter_common(h, title, chapter, next, e);
  if (st != CML_OK) return st;

  char *noext = path_join2(e->title_dir_path, e->chapter_dir_name);
  if (!noext) return CML_ERR_OOM;

  char *final = path_with_ext(noext, ".cbz");
  char *tmp = path_with_ext(noext, ".cbz.part");
  free(noext);
  if (!final || !tmp) {
    free(final);
    free(tmp);
    return CML_ERR_OOM;
  }
  e->cbz_path = final;

  if (cml_exists(e->cbz_path)) {
    free(tmp);
    e->skip_all = true;
    return CML_OK;
  }

  int zerr = 0;
  e->zip = zip_open(tmp, ZIP_CREATE | ZIP_TRUNCATE, &zerr);
  if (!e->zip) {
    free(tmp);
    return CML_ERR_ZIP;
  }

  free(tmp);
  return CML_OK;
}

cml_status cml_export_cbz_add(cml_exporter *e, const uint8_t *data, size_t len, int is_range, uint32_t start,
                              uint32_t stop) {
  if (!e || !e->zip || !data) return CML_ERR_INVALID;
  char *filename = NULL;
  cml_status st = cml_format_page_filename(e->chapter_prefix, e->chapter_suffix, is_range, start, stop, "jpg", &filename);
  if (st != CML_OK) return st;

  char *internal = path_join2(e->chapter_dir_name, filename);
  free(filename);
  if (!internal) return CML_ERR_OOM;

  uint8_t *copy = (uint8_t *)malloc(len);
  if (!copy) {
    free(internal);
    return CML_ERR_OOM;
  }
  memcpy(copy, data, len);

  zip_source_t *src = zip_source_buffer(e->zip, copy, len, 1 /* freep */);
  if (!src) {
    free(copy);
    free(internal);
    return CML_ERR_ZIP;
  }

  if (zip_file_add(e->zip, internal, src, ZIP_FL_ENC_UTF_8) < 0) {
    zip_source_free(src);
    free(internal);
    return CML_ERR_ZIP;
  }
  free(internal);
  return CML_OK;
}

// Finalize: close zip and rename .part -> .cbz
static cml_status cbz_finalize(cml_exporter *e) {
  if (!e || !e->zip || !e->cbz_path) return CML_ERR_INVALID;
  const char *dst = e->cbz_path;
  size_t n = strlen(dst) + 6;
  char *src = (char *)malloc(n);
  if (!src) return CML_ERR_OOM;
  snprintf(src, n, "%s.part", dst);

  if (zip_close(e->zip) != 0) {
    e->zip = NULL;
    free(src);
    return CML_ERR_ZIP;
  }
  e->zip = NULL;
  cml_status st = cml_rename_overwrite(src, dst);
  if (st != CML_OK) unlink(src);
  free(src);
  return st;
}

// Called by the shared exporter close path.
cml_status cml_export_cbz_finalize(cml_exporter *e) { return cbz_finalize(e); }

void cml_export_cbz_abort(cml_exporter *e) {
  if (!e) return;
  if (e->zip) {
    zip_discard(e->zip);
    e->zip = NULL;
  }
  if (e->cbz_path) {
    size_t n = strlen(e->cbz_path) + 6;
    char *part = (char *)malloc(n);
    if (!part) return;
    snprintf(part, n, "%s.part", e->cbz_path);
    unlink(part);
    free(part);
  }
}
