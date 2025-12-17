#include "cml_internal.h"

#include <stdlib.h>

static int u32_cmp(const void *a, const void *b) {
  uint32_t ua = *(const uint32_t *)a;
  uint32_t ub = *(const uint32_t *)b;
  if (ua < ub) return -1;
  if (ua > ub) return 1;
  return 0;
}

int cml_u32_push(cml_u32_vec *v, uint32_t x) {
  if (!v) return -1;
  if (v->len == v->cap) {
    size_t next = v->cap ? (v->cap * 2) : 16;
    void *p = realloc(v->items, next * sizeof(uint32_t));
    if (!p) return -1;
    v->items = (uint32_t *)p;
    v->cap = next;
  }
  v->items[v->len++] = x;
  return 0;
}

int cml_u32_sort_dedupe(cml_u32_vec *v) {
  if (!v) return -1;
  if (v->len < 2) return 0;
  qsort(v->items, v->len, sizeof(uint32_t), u32_cmp);
  size_t out = 1;
  for (size_t i = 1; i < v->len; i++) {
    if (v->items[i] != v->items[out - 1]) v->items[out++] = v->items[i];
  }
  v->len = out;
  return 0;
}

void cml_u32_free(cml_u32_vec *v) {
  if (!v) return;
  free(v->items);
  v->items = NULL;
  v->len = 0;
  v->cap = 0;
}

