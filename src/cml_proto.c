#include "cml_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  const uint8_t *p;
  size_t len;
  size_t off;
} pb;

static int pb_has(const pb *c, size_t n) { return c->off + n <= c->len; }

static int pb_varint(pb *c, uint64_t *out) {
  uint64_t v = 0;
  int shift = 0;
  while (1) {
    if (!pb_has(c, 1) || shift > 63) return 0;
    uint8_t b = c->p[c->off++];
    v |= (uint64_t)(b & 0x7fu) << shift;
    if ((b & 0x80u) == 0) break;
    shift += 7;
  }
  *out = v;
  return 1;
}

static int pb_tag(pb *c, uint32_t *field_no, uint32_t *wire_type) {
  uint64_t tag = 0;
  if (!pb_varint(c, &tag)) return 0;
  *field_no = (uint32_t)(tag >> 3);
  *wire_type = (uint32_t)(tag & 0x7);
  return 1;
}

static int pb_len(pb *c, const uint8_t **out, size_t *out_len) {
  uint64_t n = 0;
  if (!pb_varint(c, &n)) return 0;
  if (n > SIZE_MAX) return 0;
  if (!pb_has(c, (size_t)n)) return 0;
  *out = c->p + c->off;
  *out_len = (size_t)n;
  c->off += (size_t)n;
  return 1;
}

static int pb_skip(pb *c, uint32_t wt) {
  switch (wt) {
    case 0: {
      uint64_t v = 0;
      return pb_varint(c, &v);
    }
    case 1:
      if (!pb_has(c, 8)) return 0;
      c->off += 8;
      return 1;
    case 2: {
      const uint8_t *p = NULL;
      size_t n = 0;
      return pb_len(c, &p, &n);
    }
    case 5:
      if (!pb_has(c, 4)) return 0;
      c->off += 4;
      return 1;
    default:
      return 0;
  }
}

static char *dup_cstr(const uint8_t *p, size_t n) {
  char *s = (char *)malloc(n + 1);
  if (!s) return NULL;
  memcpy(s, p, n);
  s[n] = '\0';
  return s;
}

static void free_chapter(cml_chapter *c) {
  if (!c) return;
  free(c->name);
  free(c->sub_title);
  memset(c, 0, sizeof(*c));
}

static void free_title(cml_title *t) {
  if (!t) return;
  free(t->name);
  free(t->author);
  memset(t, 0, sizeof(*t));
}

static void free_manga_page(cml_manga_page *p) {
  if (!p) return;
  free(p->image_url);
  free(p->encryption_key);
  memset(p, 0, sizeof(*p));
}

static void free_last_page(cml_last_page *p) {
  if (!p) return;
  free_chapter(&p->current_chapter);
  free_chapter(&p->next_chapter);
  memset(p, 0, sizeof(*p));
}

static void free_page(cml_page *p) {
  if (!p) return;
  if (p->has_manga_page) free_manga_page(&p->manga_page);
  if (p->has_last_page) free_last_page(&p->last_page);
  memset(p, 0, sizeof(*p));
}

static cml_status parse_chapter(const uint8_t *buf, size_t len, cml_chapter *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 2 && wt == 0) {
      uint64_t v = 0;
      if (!pb_varint(&c, &v)) return CML_ERR_PROTO;
      out->chapter_id = (uint32_t)v;
      continue;
    }
    if (f == 3 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->name);
      out->name = dup_cstr(p, n);
      if (!out->name) return CML_ERR_OOM;
      continue;
    }
    if (f == 4 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->sub_title);
      out->sub_title = dup_cstr(p, n);
      if (!out->sub_title) return CML_ERR_OOM;
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static cml_status parse_manga_page(const uint8_t *buf, size_t len, cml_manga_page *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 1 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->image_url);
      out->image_url = dup_cstr(p, n);
      if (!out->image_url) return CML_ERR_OOM;
      continue;
    }
    if (f == 4 && wt == 0) {
      uint64_t v = 0;
      if (!pb_varint(&c, &v)) return CML_ERR_PROTO;
      out->type = (int32_t)v;
      continue;
    }
    if (f == 5 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->encryption_key);
      out->encryption_key = dup_cstr(p, n);
      if (!out->encryption_key) return CML_ERR_OOM;
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static cml_status parse_last_page(const uint8_t *buf, size_t len, cml_last_page *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 1 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free_chapter(&out->current_chapter);
      cml_status st = parse_chapter(p, n, &out->current_chapter);
      if (st != CML_OK) return st;
      continue;
    }
    if (f == 2 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free_chapter(&out->next_chapter);
      cml_status st = parse_chapter(p, n, &out->next_chapter);
      if (st != CML_OK) return st;
      out->has_next_chapter = (out->next_chapter.chapter_id != 0);
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static cml_status append_page(cml_manga_viewer *v, const cml_page *p) {
  size_t next = v->pages_len + 1;
  cml_page *pp = (cml_page *)realloc(v->pages, next * sizeof(cml_page));
  if (!pp) return CML_ERR_OOM;
  v->pages = pp;
  v->pages[v->pages_len++] = *p;
  return CML_OK;
}

static cml_status append_chapter_meta(cml_manga_viewer *v, const cml_chapter *c) {
  size_t next = v->chapters_len + 1;
  cml_chapter *cc = (cml_chapter *)realloc(v->chapters, next * sizeof(cml_chapter));
  if (!cc) return CML_ERR_OOM;
  v->chapters = cc;
  v->chapters[v->chapters_len++] = *c;
  return CML_OK;
}

static cml_status parse_page(const uint8_t *buf, size_t len, cml_page *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 1 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      cml_status st = parse_manga_page(p, n, &out->manga_page);
      if (st != CML_OK) return st;
      out->has_manga_page = true;
      continue;
    }
    if (f == 3 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      cml_status st = parse_last_page(p, n, &out->last_page);
      if (st != CML_OK) return st;
      out->has_last_page = true;
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static cml_status parse_manga_viewer(const uint8_t *buf, size_t len, cml_manga_viewer *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 1 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      cml_page page = {0};
      cml_status st = parse_page(p, n, &page);
      if (st != CML_OK) return st;
      st = append_page(out, &page);
      if (st != CML_OK) {
        free_page(&page);
        return st;
      }
      continue;
    }
    if (f == 2 && wt == 0) {
      uint64_t v = 0;
      if (!pb_varint(&c, &v)) return CML_ERR_PROTO;
      out->chapter_id = (uint32_t)v;
      continue;
    }
    if (f == 3 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      cml_chapter ch = {0};
      cml_status st = parse_chapter(p, n, &ch);
      if (st != CML_OK) return st;
      st = append_chapter_meta(out, &ch);
      if (st != CML_OK) {
        free_chapter(&ch);
        return st;
      }
      continue;
    }
    if (f == 6 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->chapter_name);
      out->chapter_name = dup_cstr(p, n);
      if (!out->chapter_name) return CML_ERR_OOM;
      continue;
    }
    if (f == 9 && wt == 0) {
      uint64_t v = 0;
      if (!pb_varint(&c, &v)) return CML_ERR_PROTO;
      out->title_id = (uint32_t)v;
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static cml_status find_len_field(const uint8_t *buf, size_t len, uint32_t want_field, const uint8_t **out,
                                 size_t *out_len) {
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == want_field && wt == 2) {
      if (!pb_len(&c, out, out_len)) return CML_ERR_PROTO;
      return CML_OK;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_ERR_PROTO;
}

static cml_status parse_title(const uint8_t *buf, size_t len, cml_title *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 1 && wt == 0) {
      uint64_t v = 0;
      if (!pb_varint(&c, &v)) return CML_ERR_PROTO;
      out->title_id = (uint32_t)v;
      continue;
    }
    if (f == 2 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->name);
      out->name = dup_cstr(p, n);
      if (!out->name) return CML_ERR_OOM;
      continue;
    }
    if (f == 3 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free(out->author);
      out->author = dup_cstr(p, n);
      if (!out->author) return CML_ERR_OOM;
      continue;
    }
    if (f == 7 && wt == 0) {
      uint64_t v = 0;
      if (!pb_varint(&c, &v)) return CML_ERR_PROTO;
      out->language = (int32_t)v;
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static cml_status append_group_chapter(cml_chapter **arr, size_t *len, const cml_chapter *c) {
  size_t next = *len + 1;
  cml_chapter *cc = (cml_chapter *)realloc(*arr, next * sizeof(cml_chapter));
  if (!cc) return CML_ERR_OOM;
  *arr = cc;
  (*arr)[(*len)++] = *c;
  return CML_OK;
}

static cml_status parse_chapter_group(const uint8_t *buf, size_t len, cml_chapter_group *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if ((f == 2 || f == 4) && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      cml_chapter ch = {0};
      cml_status st = parse_chapter(p, n, &ch);
      if (st != CML_OK) return st;
      if (f == 2) {
        st = append_group_chapter(&out->first, &out->first_len, &ch);
      } else {
        st = append_group_chapter(&out->last, &out->last_len, &ch);
      }
      if (st != CML_OK) {
        free_chapter(&ch);
        return st;
      }
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

static void free_group(cml_chapter_group *g) {
  if (!g) return;
  for (size_t i = 0; i < g->first_len; i++) free_chapter(&g->first[i]);
  for (size_t i = 0; i < g->last_len; i++) free_chapter(&g->last[i]);
  free(g->first);
  free(g->last);
  memset(g, 0, sizeof(*g));
}

static cml_status append_group(cml_title_detail *d, const cml_chapter_group *g) {
  size_t next = d->groups_len + 1;
  cml_chapter_group *gg = (cml_chapter_group *)realloc(d->groups, next * sizeof(cml_chapter_group));
  if (!gg) return CML_ERR_OOM;
  d->groups = gg;
  d->groups[d->groups_len++] = *g;
  return CML_OK;
}

static cml_status parse_title_detail_view(const uint8_t *buf, size_t len, cml_title_detail *out) {
  memset(out, 0, sizeof(*out));
  pb c = {.p = buf, .len = len, .off = 0};
  while (c.off < c.len) {
    uint32_t f = 0, wt = 0;
    if (!pb_tag(&c, &f, &wt)) return CML_ERR_PROTO;
    if (f == 1 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      free_title(&out->title);
      cml_status st = parse_title(p, n, &out->title);
      if (st != CML_OK) return st;
      continue;
    }
    if (f == 28 && wt == 2) {
      const uint8_t *p = NULL;
      size_t n = 0;
      if (!pb_len(&c, &p, &n)) return CML_ERR_PROTO;
      cml_chapter_group g = {0};
      cml_status st = parse_chapter_group(p, n, &g);
      if (st != CML_OK) return st;
      st = append_group(out, &g);
      if (st != CML_OK) {
        free_group(&g);
        return st;
      }
      continue;
    }
    if (!pb_skip(&c, wt)) return CML_ERR_PROTO;
  }
  return CML_OK;
}

cml_status cml_proto_parse_manga_viewer(const uint8_t *buf, size_t len, cml_manga_viewer *out) {
  if (!buf || !out) return CML_ERR_INVALID;
  const uint8_t *success = NULL;
  size_t success_len = 0;
  cml_status st = find_len_field(buf, len, 1 /* Response.success */, &success, &success_len);
  if (st != CML_OK) return st;
  const uint8_t *mv = NULL;
  size_t mv_len = 0;
  st = find_len_field(success, success_len, 10 /* SuccessResult.manga_viewer */, &mv, &mv_len);
  if (st != CML_OK) return st;
  return parse_manga_viewer(mv, mv_len, out);
}

cml_status cml_proto_parse_title_detail(const uint8_t *buf, size_t len, cml_title_detail *out) {
  if (!buf || !out) return CML_ERR_INVALID;
  const uint8_t *success = NULL;
  size_t success_len = 0;
  cml_status st = find_len_field(buf, len, 1 /* Response.success */, &success, &success_len);
  if (st != CML_OK) return st;
  const uint8_t *tdv = NULL;
  size_t tdv_len = 0;
  st = find_len_field(success, success_len, 8 /* SuccessResult.title_detail_view */, &tdv, &tdv_len);
  if (st != CML_OK) return st;
  return parse_title_detail_view(tdv, tdv_len, out);
}

void cml_proto_free_manga_viewer(cml_manga_viewer *v) {
  if (!v) return;
  for (size_t i = 0; i < v->pages_len; i++) free_page(&v->pages[i]);
  for (size_t i = 0; i < v->chapters_len; i++) free_chapter(&v->chapters[i]);
  free(v->pages);
  free(v->chapters);
  free(v->chapter_name);
  memset(v, 0, sizeof(*v));
}

void cml_proto_free_title_detail(cml_title_detail *d) {
  if (!d) return;
  free_title(&d->title);
  for (size_t i = 0; i < d->groups_len; i++) free_group(&d->groups[i]);
  free(d->groups);
  memset(d, 0, sizeof(*d));
}

