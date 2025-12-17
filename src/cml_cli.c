#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cml/cml.h"

static void print_help(FILE *out) {
  fputs(
      "Usage: cml [OPTIONS] [URLS]...\n"
      "\n"
      "C MANGA Plus Loader\n"
      "\n"
      "Options:\n"
      "  --version                       Show version and exit.\n"
      "  -o, --out <directory>           Output directory  [default: cml_downloads]\n"
      "  -r, --raw                       Write raw images instead of CBZ\n"
      "  -q, --quality <super_high|high|low>\n"
      "                                  Image quality  [default: super_high]\n"
      "  -s, --split                     Request server-side split for combined images\n"
      "  -c, --chapter <id>              Chapter id (repeatable)\n"
      "  -t, --title <id>                Title id (repeatable)\n"
      "  -b, --begin <n>                 Minimal chapter number to download  [default: 0]\n"
      "  -e, --end <n>                   Maximal chapter number to download\n"
      "  -l, --last                      Download only the last chapter for each title\n"
      "      --chapter-title             Include chapter titles in filenames\n"
      "      --chapter-subdir            Save raw images in a per-chapter subdirectory\n"
      "  -h, --help                      Show this message and exit.\n"
      "\n"
      "Environment:\n"
      "  CML_OUT_DIR, CML_RAW, CML_QUALITY\n",
      out);
}

static int parse_u32(const char *s, uint32_t *out) {
  if (!s || !out) return 0;
  char *end = NULL;
  unsigned long v = strtoul(s, &end, 10);
  if (!end || end == s || *end != '\0') return 0;
  if (v > 0xffffffffUL) return 0;
  *out = (uint32_t)v;
  return 1;
}

static int parse_quality(const char *s, cml_quality *out) {
  if (!s || !out) return 0;
  if (strcmp(s, "super_high") == 0) {
    *out = CML_QUALITY_SUPER_HIGH;
    return 1;
  }
  if (strcmp(s, "high") == 0) {
    *out = CML_QUALITY_HIGH;
    return 1;
  }
  if (strcmp(s, "low") == 0) {
    *out = CML_QUALITY_LOW;
    return 1;
  }
  return 0;
}

static int env_bool(const char *key, int fallback) {
  const char *v = getenv(key);
  if (!v || !*v) return fallback;
  if (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0) return 1;
  if (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0 || strcasecmp(v, "no") == 0) return 0;
  return fallback;
}

static void default_log(void *user, const cml_log_event *ev) {
  (void)user;
  const char *lvl = "INFO";
  if (ev->level == CML_LOG_ERROR) lvl = "ERROR";
  else if (ev->level == CML_LOG_WARN) lvl = "WARN";
  else if (ev->level == CML_LOG_DEBUG) lvl = "DEBUG";
  fprintf(stderr, "[%s] %s\n", lvl, ev->message ? ev->message : "");
}

static void default_progress(void *user, const cml_progress_event *ev) {
  (void)user;
  if (!ev || !ev->stage) return;
  if (ev->total) {
    fprintf(stderr, "\r[%s] %u/%u", ev->stage, ev->done, ev->total);
  } else {
    fprintf(stderr, "\r[%s] %u", ev->stage, ev->done);
  }
  fflush(stderr);
  if (ev->total && ev->done == ev->total) fputc('\n', stderr);
}

typedef struct {
  uint32_t *items;
  size_t len;
  size_t cap;
} u32_list;

static int u32_list_push(u32_list *l, uint32_t v) {
  if (l->len == l->cap) {
    size_t next = l->cap ? (l->cap * 2) : 16;
    void *p = realloc(l->items, next * sizeof(uint32_t));
    if (!p) return 0;
    l->items = (uint32_t *)p;
    l->cap = next;
  }
  l->items[l->len++] = v;
  return 1;
}

static void u32_list_free(u32_list *l) {
  free(l->items);
  l->items = NULL;
  l->len = 0;
  l->cap = 0;
}

int main(int argc, char **argv) {
  const char *out_dir = getenv("CML_OUT_DIR");
  if (!out_dir || !*out_dir) out_dir = "cml_downloads";
  cml_output_format output = env_bool("CML_RAW", 0) ? CML_OUTPUT_RAW : CML_OUTPUT_CBZ;
  cml_quality quality = CML_QUALITY_SUPER_HIGH;
  const char *qenv = getenv("CML_QUALITY");
  if (qenv && *qenv) {
    if (!parse_quality(qenv, &quality)) {
      fprintf(stderr, "cml: invalid $CML_QUALITY (expected super_high|high|low)\n");
      return 1;
    }
  }

  cml_config cfg = {
      .out_dir = out_dir,
      .output = output,
      .quality = quality,
      .split = false,
      .min_chapter = 0,
      .max_chapter = 0,
      .last_only = false,
      .include_chapter_title = false,
      .chapter_subdir = false,
      .log_fn = default_log,
      .progress_fn = default_progress,
      .user = NULL,
  };

  u32_list chapter_ids = {0};
  u32_list title_ids = {0};

  enum { OPT_CHAPTER_TITLE = 1000, OPT_CHAPTER_SUBDIR = 1001 };
  static struct option longopts[] = {
      {"out", required_argument, NULL, 'o'},
      {"raw", no_argument, NULL, 'r'},
      {"quality", required_argument, NULL, 'q'},
      {"split", no_argument, NULL, 's'},
      {"chapter", required_argument, NULL, 'c'},
      {"title", required_argument, NULL, 't'},
      {"begin", required_argument, NULL, 'b'},
      {"end", required_argument, NULL, 'e'},
      {"last", no_argument, NULL, 'l'},
      {"chapter-title", no_argument, NULL, OPT_CHAPTER_TITLE},
      {"chapter-subdir", no_argument, NULL, OPT_CHAPTER_SUBDIR},
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'V'},
      {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "o:rq:sc:t:b:e:lhV", longopts, NULL)) != -1) {
    switch (opt) {
      case 'o':
        cfg.out_dir = optarg;
        break;
      case 'r':
        cfg.output = CML_OUTPUT_RAW;
        break;
      case 'q':
        if (!parse_quality(optarg, &cfg.quality)) {
          fprintf(stderr, "cml: invalid --quality (expected super_high|high|low)\n");
          return 1;
        }
        break;
      case 's':
        cfg.split = true;
        break;
      case 'b': {
        uint32_t v = 0;
        if (!parse_u32(optarg, &v)) {
          fprintf(stderr, "cml: invalid --begin\n");
          return 1;
        }
        cfg.min_chapter = v;
        break;
      }
      case 'e': {
        uint32_t v = 0;
        if (!parse_u32(optarg, &v) || v < 1) {
          fprintf(stderr, "cml: invalid --end (expected integer >= 1)\n");
          return 1;
        }
        cfg.max_chapter = v;
        break;
      }
      case 'l':
        cfg.last_only = true;
        break;
      case OPT_CHAPTER_TITLE:
        cfg.include_chapter_title = true;
        break;
      case OPT_CHAPTER_SUBDIR:
        cfg.chapter_subdir = true;
        break;
      case 'h':
        print_help(stdout);
        u32_list_free(&chapter_ids);
        u32_list_free(&title_ids);
        return 0;
      case 'V':
        printf("cml 0.1.0\n");
        u32_list_free(&chapter_ids);
        u32_list_free(&title_ids);
        return 0;
      case 'c':
      case 't': {
        uint32_t id = 0;
        if (!parse_u32(optarg, &id) || id == 0) {
          fprintf(stderr, "cml: invalid id\n");
          u32_list_free(&chapter_ids);
          u32_list_free(&title_ids);
          return 1;
        }
        if (opt == 'c') {
          if (!u32_list_push(&chapter_ids, id)) {
            fprintf(stderr, "cml: out of memory\n");
            u32_list_free(&chapter_ids);
            u32_list_free(&title_ids);
            return 1;
          }
        } else {
          if (!u32_list_push(&title_ids, id)) {
            fprintf(stderr, "cml: out of memory\n");
            u32_list_free(&chapter_ids);
            u32_list_free(&title_ids);
            return 1;
          }
        }
        break;
      }
      default:
        u32_list_free(&chapter_ids);
        u32_list_free(&title_ids);
        return 1;
    }
  }

  cml *h = cml_create(&cfg);
  if (!h) {
    fprintf(stderr, "cml: failed to initialize\n");
    u32_list_free(&chapter_ids);
    u32_list_free(&title_ids);
    return 1;
  }

  for (size_t i = 0; i < chapter_ids.len; i++) {
    cml_status st = cml_add_chapter_id(h, chapter_ids.items[i]);
    if (st != CML_OK) {
      fprintf(stderr, "cml: %s\n", cml_status_string(st));
      cml_destroy(h);
      u32_list_free(&chapter_ids);
      u32_list_free(&title_ids);
      return 1;
    }
  }
  for (size_t i = 0; i < title_ids.len; i++) {
    cml_status st = cml_add_title_id(h, title_ids.items[i]);
    if (st != CML_OK) {
      fprintf(stderr, "cml: %s\n", cml_status_string(st));
      cml_destroy(h);
      u32_list_free(&chapter_ids);
      u32_list_free(&title_ids);
      return 1;
    }
  }
  u32_list_free(&chapter_ids);
  u32_list_free(&title_ids);

  for (int i = optind; i < argc; i++) {
    cml_status st = cml_add_url(h, argv[i]);
    if (st != CML_OK) {
      fprintf(stderr, "cml: invalid url: %s\n", argv[i]);
      cml_destroy(h);
      return 1;
    }
  }

  cml_status st = cml_run(h);
  if (st != CML_OK) fprintf(stderr, "cml: %s\n", cml_status_string(st));
  cml_destroy(h);
  return (st == CML_OK) ? 0 : 1;
}
