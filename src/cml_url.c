#include "cml_internal.h"

#include <string.h>

static const char *find_after(const char *s, const char *needle) {
  const char *p = strstr(s, needle);
  if (!p) return NULL;
  return p + strlen(needle);
}

static int parse_u32_prefix(const char *p, uint32_t *out) {
  if (!p || !out) return 0;
  uint64_t v = 0;
  size_t n = 0;
  while (p[n] >= '0' && p[n] <= '9') {
    v = v * 10 + (uint64_t)(p[n] - '0');
    if (v > UINT32_MAX) return 0;
    n++;
  }
  if (n == 0) return 0;
  *out = (uint32_t)v;
  return 1;
}

int cml_url_extract_viewer_id(const char *s, uint32_t *out) {
  if (!s || !out) return 0;
  const char *p = find_after(s, "/viewer/");
  if (!p) p = find_after(s, "viewer/");
  if (!p) return 0;
  return parse_u32_prefix(p, out);
}

int cml_url_extract_titles_id(const char *s, uint32_t *out) {
  if (!s || !out) return 0;
  const char *p = find_after(s, "/titles/");
  if (!p) p = find_after(s, "titles/");
  if (!p) return 0;
  return parse_u32_prefix(p, out);
}

