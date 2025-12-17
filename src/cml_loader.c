#include "cml_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint32_t title_id;
  cml_chapter *chapters;
  size_t chapters_len;
  size_t chapters_cap;
} title_entry;

typedef struct {
  title_entry *items;
  size_t len;
  size_t cap;
} title_map;

static void chapter_free_fields(cml_chapter *c) {
  if (!c) return;
  free(c->name);
  free(c->sub_title);
  c->name = NULL;
  c->sub_title = NULL;
}

static void map_free(title_map *m) {
  if (!m) return;
  for (size_t i = 0; i < m->len; i++) {
    for (size_t j = 0; j < m->items[i].chapters_len; j++) chapter_free_fields(&m->items[i].chapters[j]);
    free(m->items[i].chapters);
  }
  free(m->items);
  memset(m, 0, sizeof(*m));
}

static title_entry *map_get(title_map *m, uint32_t title_id) {
  for (size_t i = 0; i < m->len; i++) {
    if (m->items[i].title_id == title_id) return &m->items[i];
  }
  if (m->len == m->cap) {
    size_t next = m->cap ? (m->cap * 2) : 8;
    void *p = realloc(m->items, next * sizeof(title_entry));
    if (!p) return NULL;
    m->items = (title_entry *)p;
    m->cap = next;
  }
  title_entry *e = &m->items[m->len++];
  memset(e, 0, sizeof(*e));
  e->title_id = title_id;
  return e;
}

static cml_status entry_add_chapter(title_entry *e, const cml_chapter *src) {
  if (e->chapters_len == e->chapters_cap) {
    size_t next = e->chapters_cap ? (e->chapters_cap * 2) : 16;
    void *p = realloc(e->chapters, next * sizeof(cml_chapter));
    if (!p) return CML_ERR_OOM;
    e->chapters = (cml_chapter *)p;
    e->chapters_cap = next;
  }
  cml_chapter dst = *src;
  dst.name = src->name ? strdup(src->name) : NULL;
  dst.sub_title = src->sub_title ? strdup(src->sub_title) : NULL;
  if ((src->name && !dst.name) || (src->sub_title && !dst.sub_title)) {
    free(dst.name);
    free(dst.sub_title);
    return CML_ERR_OOM;
  }
  e->chapters[e->chapters_len++] = dst;
  return CML_OK;
}

typedef struct {
  uint32_t id;
  cml_manga_viewer viewer;
} viewer_cache_entry;

typedef struct {
  viewer_cache_entry *items;
  size_t len;
  size_t cap;
} viewer_cache;

static void viewer_cache_free(viewer_cache *c) {
  if (!c) return;
  for (size_t i = 0; i < c->len; i++) cml_proto_free_manga_viewer(&c->items[i].viewer);
  free(c->items);
  memset(c, 0, sizeof(*c));
}

static cml_status viewer_cached_get(cml *h, viewer_cache *c, uint32_t chapter_id, cml_manga_viewer **out) {
  for (size_t i = 0; i < c->len; i++) {
    if (c->items[i].id == chapter_id) {
      *out = &c->items[i].viewer;
      return CML_OK;
    }
  }
  if (c->len == c->cap) {
    size_t next = c->cap ? (c->cap * 2) : 8;
    void *p = realloc(c->items, next * sizeof(viewer_cache_entry));
    if (!p) return CML_ERR_OOM;
    c->items = (viewer_cache_entry *)p;
    c->cap = next;
  }
  viewer_cache_entry *e = &c->items[c->len++];
  memset(e, 0, sizeof(*e));
  e->id = chapter_id;
  cml_status st = cml_api_get_manga_viewer(h, chapter_id, &e->viewer);
  if (st != CML_OK) return st;
  *out = &e->viewer;
  return CML_OK;
}

typedef struct {
  uint32_t id;
  cml_title_detail detail;
} detail_cache_entry;

typedef struct {
  detail_cache_entry *items;
  size_t len;
  size_t cap;
} detail_cache;

static void detail_cache_free(detail_cache *c) {
  if (!c) return;
  for (size_t i = 0; i < c->len; i++) cml_proto_free_title_detail(&c->items[i].detail);
  free(c->items);
  memset(c, 0, sizeof(*c));
}

static cml_status detail_cached_get(cml *h, detail_cache *c, uint32_t title_id, cml_title_detail **out) {
  for (size_t i = 0; i < c->len; i++) {
    if (c->items[i].id == title_id) {
      *out = &c->items[i].detail;
      return CML_OK;
    }
  }
  if (c->len == c->cap) {
    size_t next = c->cap ? (c->cap * 2) : 8;
    void *p = realloc(c->items, next * sizeof(detail_cache_entry));
    if (!p) return CML_ERR_OOM;
    c->items = (detail_cache_entry *)p;
    c->cap = next;
  }
  detail_cache_entry *e = &c->items[c->len++];
  memset(e, 0, sizeof(*e));
  e->id = title_id;
  cml_status st = cml_api_get_title_detail(h, title_id, &e->detail);
  if (st != CML_OK) return st;
  *out = &e->detail;
  return CML_OK;
}

static int remove_u32(uint32_t *arr, size_t *len, uint32_t id) {
  for (size_t i = 0; i < *len; i++) {
    if (arr[i] == id) {
      memmove(&arr[i], &arr[i + 1], (*len - i - 1) * sizeof(uint32_t));
      (*len)--;
      return 1;
    }
  }
  return 0;
}

static cml_status normalize_inputs(cml *h, viewer_cache *vc, detail_cache *dc, title_map *out) {
  memset(out, 0, sizeof(*out));
  cml_status st = CML_OK;
  size_t titles_len = h->title_ids.len;
  uint32_t *titles = NULL;
  if (titles_len) {
    titles = (uint32_t *)malloc(titles_len * sizeof(uint32_t));
    if (!titles) return CML_ERR_OOM;
    memcpy(titles, h->title_ids.items, titles_len * sizeof(uint32_t));
  }

  for (size_t i = 0; i < h->chapter_ids.len; i++) {
    uint32_t cid = h->chapter_ids.items[i];
    cml_manga_viewer *viewer = NULL;
    st = viewer_cached_get(h, vc, cid, &viewer);
    if (st != CML_OK) goto fail;

    uint32_t tid = viewer->title_id;
    title_entry *e = map_get(out, tid);
    if (!e) {
      st = CML_ERR_OOM;
      goto fail;
    }

    if (remove_u32(titles, &titles_len, tid)) {
      for (size_t j = 0; j < viewer->chapters_len; j++) {
        st = entry_add_chapter(e, &viewer->chapters[j]);
        if (st != CML_OK) goto fail;
      }
    } else {
      cml_chapter meta = {.chapter_id = viewer->chapter_id, .name = viewer->chapter_name, .sub_title = NULL};
      st = entry_add_chapter(e, &meta);
      if (st != CML_OK) goto fail;
    }
  }

  for (size_t i = 0; i < titles_len; i++) {
    uint32_t tid = titles[i];
    cml_title_detail *detail = NULL;
    st = detail_cached_get(h, dc, tid, &detail);
    if (st != CML_OK) goto fail;
    title_entry *e = map_get(out, tid);
    if (!e) {
      st = CML_ERR_OOM;
      goto fail;
    }
    for (size_t g = 0; g < detail->groups_len; g++) {
      for (size_t j = 0; j < detail->groups[g].first_len; j++) {
        st = entry_add_chapter(e, &detail->groups[g].first[j]);
        if (st != CML_OK) goto fail;
      }
      for (size_t j = 0; j < detail->groups[g].last_len; j++) {
        st = entry_add_chapter(e, &detail->groups[g].last[j]);
        if (st != CML_OK) goto fail;
      }
    }
  }

  free(titles);

  uint32_t max_ch = (h->cfg.max_chapter == 0) ? UINT32_MAX : h->cfg.max_chapter;
  for (size_t i = 0; i < out->len; i++) {
    title_entry *e = &out->items[i];
    if (e->chapters_len == 0) continue;
    if (h->cfg.last_only) {
      for (size_t j = 0; j + 1 < e->chapters_len; j++) chapter_free_fields(&e->chapters[j]);
      cml_chapter last = e->chapters[e->chapters_len - 1];
      e->chapters[0] = last;
      e->chapters_len = 1;
      continue;
    }
    size_t out_idx = 0;
    for (size_t j = 0; j < e->chapters_len; j++) {
      cml_chapter *c = &e->chapters[j];
      uint32_t chap_no = 0;
      int n = 0;
      if (c->name && cml_chapter_name_to_int(c->name, &n)) chap_no = (uint32_t)n;
      if (chap_no < h->cfg.min_chapter || chap_no > max_ch) {
        chapter_free_fields(c);
        continue;
      }
      if (out_idx != j) e->chapters[out_idx] = e->chapters[j];
      out_idx++;
    }
    e->chapters_len = out_idx;
  }

  return CML_OK;

fail:
  free(titles);
  map_free(out);
  return st;
}

static const cml_last_page *viewer_last_page(const cml_manga_viewer *v) {
  for (size_t i = v->pages_len; i > 0; i--) {
    const cml_page *p = &v->pages[i - 1];
    if (p->has_last_page) return &p->last_page;
  }
  return NULL;
}

static cml_status download_one_chapter(cml *h, const cml_title *title, uint32_t chapter_id, viewer_cache *vc) {
  cml_manga_viewer *viewer = NULL;
  cml_status st = viewer_cached_get(h, vc, chapter_id, &viewer);
  if (st != CML_OK) return st;

  const cml_last_page *lp = viewer_last_page(viewer);
  if (!lp) return CML_ERR_PROTO;

  cml_exporter *exp = NULL;
  st = cml_exporter_open(h, title, &lp->current_chapter, lp->has_next_chapter ? &lp->next_chapter : NULL, &exp);
  if (st != CML_OK) return st;

  uint32_t total = 0;
  for (size_t i = 0; i < viewer->pages_len; i++) {
    if (viewer->pages[i].has_manga_page && viewer->pages[i].manga_page.image_url && viewer->pages[i].manga_page.image_url[0])
      total++;
  }

  uint32_t done = 0;
  uint32_t page_no = 0;
  for (size_t i = 0; i < viewer->pages_len; i++) {
    const cml_page *p = &viewer->pages[i];
    if (!p->has_manga_page || !p->manga_page.image_url || !p->manga_page.image_url[0]) continue;
    done++;

    int is_range = (p->manga_page.type == 3);
    uint32_t start = page_no;
    uint32_t stop = page_no + 1;
    page_no += is_range ? 2 : 1;

    cml_progress_event ev = {.stage = "images", .title_name = title->name, .chapter_name = viewer->chapter_name, .done = done, .total = total};
    cml_progress(h, &ev);

    if (cml_exporter_skip_image(exp, is_range, start, stop)) continue;

    cml_bytes img = {0};
    st = cml_http_get(h, p->manga_page.image_url, &img);
    if (st != CML_OK) {
      cml_bytes_free(&img);
      cml_exporter_close_destroy(exp, false);
      return st;
    }
    st = cml_decrypt_xor_hex(img.data, img.len, p->manga_page.encryption_key);
    if (st != CML_OK) {
      cml_bytes_free(&img);
      cml_exporter_close_destroy(exp, false);
      return st;
    }
    st = cml_exporter_add_image(exp, img.data, img.len, is_range, start, stop);
    cml_bytes_free(&img);
    if (st != CML_OK) {
      cml_exporter_close_destroy(exp, false);
      return st;
    }
  }

  cml_exporter_close_destroy(exp, true);
  return CML_OK;
}

cml_status cml_loader_run(cml *h) {
  if (!h) return CML_ERR_INVALID;

  viewer_cache vc = {0};
  detail_cache dc = {0};
  title_map map = {0};

  cml_status st = normalize_inputs(h, &vc, &dc, &map);
  if (st != CML_OK) {
    viewer_cache_free(&vc);
    detail_cache_free(&dc);
    map_free(&map);
    return st;
  }

  for (size_t i = 0; i < map.len; i++) {
    uint32_t title_id = map.items[i].title_id;
    cml_title_detail *detail = NULL;
    st = detail_cached_get(h, &dc, title_id, &detail);
    if (st != CML_OK) break;

    const cml_title *title = &detail->title;
    cml_log(h, CML_LOG_INFO, "manga: %s", title->name ? title->name : "(unknown)");

    cml_u32_vec chap_ids = {0};
    for (size_t j = 0; j < map.items[i].chapters_len; j++) {
      if (cml_u32_push(&chap_ids, map.items[i].chapters[j].chapter_id) != 0) {
        st = CML_ERR_OOM;
        break;
      }
    }
    if (st != CML_OK) {
      cml_u32_free(&chap_ids);
      break;
    }
    if (cml_u32_sort_dedupe(&chap_ids) != 0) {
      cml_u32_free(&chap_ids);
      st = CML_ERR_OOM;
      break;
    }

    for (size_t j = 0; j < chap_ids.len; j++) {
      uint32_t chapter_id = chap_ids.items[j];
      cml_progress_event ev = {.stage = "metadata", .title_name = title->name, .chapter_name = NULL, .done = (uint32_t)(j + 1), .total = (uint32_t)chap_ids.len};
      cml_progress(h, &ev);
      st = download_one_chapter(h, title, chapter_id, &vc);
      if (st != CML_OK) {
        cml_log(h, CML_LOG_ERROR, "failed: %s", cml_status_string(st));
        break;
      }
    }
    cml_u32_free(&chap_ids);
    if (st != CML_OK) break;
  }

  viewer_cache_free(&vc);
  detail_cache_free(&dc);
  map_free(&map);
  return st;
}
