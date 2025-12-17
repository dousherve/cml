#include "cml_internal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *s;
  size_t len;
  size_t cap;
} sbuf;

static int sb_grow(sbuf *b, size_t need) {
  if (b->len + need + 1 <= b->cap) return 1;
  size_t next = b->cap ? b->cap : 128;
  while (next < b->len + need + 1) next *= 2;
  void *p = realloc(b->s, next);
  if (!p) return 0;
  b->s = (char *)p;
  b->cap = next;
  return 1;
}

static int sb_append(sbuf *b, const char *s) {
  size_t n = strlen(s);
  if (!sb_grow(b, n)) return 0;
  memcpy(b->s + b->len, s, n);
  b->len += n;
  b->s[b->len] = '\0';
  return 1;
}

static int sb_append_ch(sbuf *b, char c) {
  if (!sb_grow(b, 1)) return 0;
  b->s[b->len++] = c;
  b->s[b->len] = '\0';
  return 1;
}

static int sb_printf(sbuf *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tmp[256];
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  if ((size_t)n < sizeof(tmp)) return sb_append(b, tmp);
  char *dyn = (char *)malloc((size_t)n + 1);
  if (!dyn) return 0;
  va_start(ap, fmt);
  vsnprintf(dyn, (size_t)n + 1, fmt, ap);
  va_end(ap);
  int ok = sb_append(b, dyn);
  free(dyn);
  return ok;
}

static const char *lang_name(int32_t code) {
  switch (code) {
    case 0:
      return "eng";
    case 1:
      return "spa";
    case 2:
      return "fre";
    case 3:
      return "ind";
    case 4:
      return "por";
    case 5:
      return "rus";
    case 6:
      return "tha";
    case 7:
      return "deu";
    case 9:
      return "vie";
    default:
      return "unk";
  }
}

char *cml_escape_component(const char *s) {
  if (!s) return NULL;
  sbuf b = {0};
  int last_space = 1;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    int is_word = (isalnum(c) || c == '_');
    if (is_word) {
      if (!sb_append_ch(&b, (char)c)) goto oom;
      last_space = 0;
    } else {
      if (!last_space) {
        if (!sb_append_ch(&b, ' ')) goto oom;
        last_space = 1;
      }
    }
  }
  if (!b.s) return strdup("");
  while (b.len > 0 && (b.s[b.len - 1] == ' ' || ispunct((unsigned char)b.s[b.len - 1]))) b.s[--b.len] = '\0';
  size_t start = 0;
  while (start < b.len && (b.s[start] == ' ' || ispunct((unsigned char)b.s[start]))) start++;
  if (start > 0) {
    memmove(b.s, b.s + start, b.len - start + 1);
    b.len -= start;
  }
  return b.s;
oom:
  free(b.s);
  return NULL;
}

char *cml_titlecase_ascii(const char *s) {
  if (!s) return NULL;
  char *out = strdup(s);
  if (!out) return NULL;
  int new_word = 1;
  for (unsigned char *p = (unsigned char *)out; *p; p++) {
    if (isalnum(*p)) {
      *p = (unsigned char)(new_word ? toupper(*p) : tolower(*p));
      new_word = 0;
    } else {
      new_word = 1;
    }
  }
  return out;
}

int cml_chapter_name_to_int(const char *s, int *out) {
  if (!s || !out) return 0;
  while (*s == '#') s++;
  if (!*s) return 0;
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (!end || end == s || *end != '\0') return 0;
  if (v < 0 || v > 1000000) return 0;
  *out = (int)v;
  return 1;
}

int cml_is_oneshot(const char *chapter_name, const char *chapter_subtitle) {
  int n = 0;
  if (cml_chapter_name_to_int(chapter_name, &n)) return 0;
  const char *inputs[2] = {chapter_name, chapter_subtitle};
  for (size_t i = 0; i < 2; i++) {
    const char *s = inputs[i];
    if (!s) continue;
    sbuf b = {0};
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
      if (!sb_append_ch(&b, (char)tolower(*p))) {
        free(b.s);
        return 0;
      }
    }
    int yes = (b.s && strstr(b.s, "one") && strstr(b.s, "shot")) ? 1 : 0;
    free(b.s);
    if (yes) return 1;
  }
  return 0;
}

static int is_extra(const char *chapter_name) {
  if (!chapter_name) return 0;
  while (*chapter_name == '#') chapter_name++;
  return strcmp(chapter_name, "ex") == 0;
}

cml_status cml_build_names(const cml_title *title, const cml_chapter *chapter, const cml_chapter *next_chapter,
                           bool include_chapter_title, char **out_title_dir, char **out_chapter_prefix,
                           char **out_chapter_suffix, char **out_chapter_dir) {
  if (!title || !chapter || !out_title_dir || !out_chapter_prefix || !out_chapter_suffix || !out_chapter_dir)
    return CML_ERR_INVALID;

  *out_title_dir = NULL;
  *out_chapter_prefix = NULL;
  *out_chapter_suffix = NULL;
  *out_chapter_dir = NULL;

  char *esc_title = cml_escape_component(title->name ? title->name : "");
  if (!esc_title) return CML_ERR_OOM;
  char *title_dir = cml_titlecase_ascii(esc_title);
  free(esc_title);
  if (!title_dir) return CML_ERR_OOM;

  int oneshot = cml_is_oneshot(chapter->name, chapter->sub_title);
  int extra = is_extra(chapter->name);

  int chapter_num = -1;
  const char *num_prefix = "";
  const char *num_suffix = "";
  char *chapter_fallback = NULL;

  if (oneshot) {
    chapter_num = 0;
  } else if (extra && next_chapter && next_chapter->name) {
    int n = 0;
    if (cml_chapter_name_to_int(next_chapter->name, &n)) {
      n -= 1;
      chapter_num = n;
      num_suffix = "x1";
      num_prefix = (n < 1000) ? "c" : "d";
    } else {
      chapter_fallback = cml_escape_component(chapter->name);
    }
  } else {
    int n = 0;
    if (cml_chapter_name_to_int(chapter->name, &n)) {
      chapter_num = n;
      num_prefix = (n < 1000) ? "c" : "d";
    } else {
      chapter_fallback = cml_escape_component(chapter->name);
    }
  }

  sbuf prefix = {0};
  if (!sb_append(&prefix, title_dir)) goto oom;
  if (title->language != 0) {
    if (!sb_printf(&prefix, " [%s]", lang_name(title->language))) goto oom;
  }
  if (!sb_append(&prefix, " - ")) goto oom;

  if (chapter_num >= 0) {
    if (!sb_printf(&prefix, "%s%03d%s", num_prefix, chapter_num, num_suffix)) goto oom;
  } else {
    const char *s = chapter_fallback ? chapter_fallback : (chapter->name ? chapter->name : "");
    size_t slen = strlen(s);
    if (slen < 3) {
      for (size_t i = 0; i < 3 - slen; i++) {
        if (!sb_append_ch(&prefix, '0')) goto oom;
      }
    }
    if (!sb_append(&prefix, num_prefix)) goto oom;
    if (!sb_append(&prefix, s)) goto oom;
    if (!sb_append(&prefix, num_suffix)) goto oom;
  }

  if (!sb_append(&prefix, " (web)")) goto oom;

  sbuf suffix = {0};
  if (oneshot) {
    if (!sb_append(&suffix, "[Oneshot] ")) goto oom;
  }
  if (include_chapter_title && chapter->sub_title && chapter->sub_title[0]) {
    char *esc = cml_escape_component(chapter->sub_title);
    if (!esc) goto oom;
    if (!sb_printf(&suffix, "[%s] ", esc)) {
      free(esc);
      goto oom;
    }
    free(esc);
  }
  if (!sb_append(&suffix, "[Unknown]")) goto oom;

  sbuf dir = {0};
  if (!sb_printf(&dir, "%s %s", prefix.s, suffix.s)) goto oom;

  *out_title_dir = title_dir;
  *out_chapter_prefix = prefix.s;
  *out_chapter_suffix = suffix.s;
  *out_chapter_dir = dir.s;
  free(chapter_fallback);
  return CML_OK;

oom:
  free(title_dir);
  free(prefix.s);
  free(suffix.s);
  free(dir.s);
  free(chapter_fallback);
  return CML_ERR_OOM;
}

cml_status cml_format_page_filename(const char *chapter_prefix, const char *chapter_suffix, int is_range,
                                    uint32_t start, uint32_t stop, const char *ext, char **out) {
  if (!chapter_prefix || !chapter_suffix || !out) return CML_ERR_INVALID;
  *out = NULL;
  const char *e = (ext && *ext) ? ext : "jpg";
  while (*e == '.') e++;
  char page[64];
  if (is_range) {
    snprintf(page, sizeof(page), "p%03u-%03u", start, stop);
  } else {
    snprintf(page, sizeof(page), "p%03u", start);
  }
  size_t n = strlen(chapter_prefix) + strlen(chapter_suffix) + strlen(page) + strlen(e) + 8;
  char *s = (char *)malloc(n);
  if (!s) return CML_ERR_OOM;
  snprintf(s, n, "%s - %s %s.%s", chapter_prefix, page, chapter_suffix, e);
  *out = s;
  return CML_OK;
}

