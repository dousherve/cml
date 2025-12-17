#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "cml/cml.h"

static const char *CML_CLI_VERSION = "0.1.0";

typedef struct {
  int tty;
  int color;
  int printed_any_title;
  int chapter_line_active;

  const char *title;
  const char *author;
  uint32_t title_done;
  uint32_t title_total;
  const char *chapter_no;
  const char *chapter_title;
  uint32_t chapter_done;
  uint32_t chapter_total;
  uint32_t pages_done;
  uint32_t pages_total;

  uint32_t chapters_done_total;
  uint64_t pages_done_total;
} cli_ui;

static int str_eq(const char *a, const char *b) {
  if (a == b) return 1;
  if (!a || !b) return 0;
  return strcmp(a, b) == 0;
}

static int ui_color_enabled(void) {
  const char *no = getenv("NO_COLOR");
  return !(no && *no);
}

static const char *c_rst(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[0m" : ""; }
static const char *c_bold(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[1m" : ""; }
static const char *c_dim(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[2m" : ""; }
static const char *c_grn(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[32m" : ""; }
static const char *c_red(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[31m" : ""; }
static const char *c_mag(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[35m" : ""; }
static const char *c_cyn(const cli_ui *ui) { return (ui && ui->color) ? "\x1b[36m" : ""; }

static void ui_clear_line(cli_ui *ui) {
  if (!ui || !ui->tty) return;
  fputs("\r\x1b[2K", stderr);
}

static void ui_end_chapter_line(cli_ui *ui) {
  if (!ui || !ui->tty) return;
  if (!ui->chapter_line_active) return;
  fputc('\n', stderr);
  ui->chapter_line_active = 0;
}

static const char *ui_manga_bullet(const cli_ui *ui) { return (ui && ui->tty) ? "•" : "*"; }
static const char *ui_chapter_bullet(const cli_ui *ui) { return (ui && ui->tty) ? "↳" : "-"; }
static const char *ui_chapter_mark_down(const cli_ui *ui) { return (ui && ui->tty) ? "↓" : "v"; }
static const char *ui_chapter_mark_done(const cli_ui *ui) { return (ui && ui->tty) ? "✓" : "OK"; }
static const char *ui_mark_ok(const cli_ui *ui) { return (ui && ui->tty) ? "✓" : "OK"; }
static const char *ui_mark_err(const cli_ui *ui) { return (ui && ui->tty) ? "✗" : "ERROR"; }

static void ui_print_prefix(cli_ui *ui, int indent_spaces, const char *bullet, uint32_t idx, uint32_t total) {
  for (int i = 0; i < indent_spaces; i++) fputc(' ', stderr);
  if (bullet && *bullet) fputs(bullet, stderr);
  fputc(' ', stderr);
  if (idx && total) fprintf(stderr, "%s%u/%u)%s ", c_dim(ui), idx, total, c_rst(ui));
}

static void ui_print_chapter_prefix(cli_ui *ui, int done) {
  ui_print_prefix(ui, 2, ui_chapter_bullet(ui), ui->chapter_done, ui->chapter_total);
  fprintf(stderr, "%s%s%s ", c_dim(ui), done ? ui_chapter_mark_done(ui) : ui_chapter_mark_down(ui), c_rst(ui));
}

static void ui_print_title(cli_ui *ui, const char *title, const char *author) {
  if (!ui || !title || !*title) return;
  ui_end_chapter_line(ui);
  fputc('\n', stderr);

  ui_print_prefix(ui, 0, ui_manga_bullet(ui), ui->title_done, ui->title_total);
  fprintf(stderr, "%s%s%s%s", c_bold(ui), c_cyn(ui), title, c_rst(ui));
  if (author && *author) fprintf(stderr, "%s — %s%s", c_dim(ui), author, c_rst(ui));
  fputc('\n', stderr);
  ui->printed_any_title = 1;
}

static void ui_render_chapter_done(cli_ui *ui, const char *chapter_no, const char *chapter_title, uint32_t pages_total) {
  if (!ui || !ui->tty) return;

  ui_clear_line(ui);
  ui_print_chapter_prefix(ui, 1);
  fprintf(stderr, "%s%sChapter%s", c_bold(ui), c_mag(ui), c_rst(ui));
  if (chapter_no && *chapter_no) fprintf(stderr, " %s%s%s", c_bold(ui), chapter_no, c_rst(ui));
  if (chapter_title && *chapter_title) fprintf(stderr, " %s—%s %s", c_dim(ui), c_rst(ui), chapter_title);
  if (pages_total) fprintf(stderr, " %s(%u pages)%s", c_dim(ui), pages_total, c_rst(ui));
  fputc('\n', stderr);
  ui->chapter_line_active = 0;
}

static void ui_render_chapter_progress(cli_ui *ui, uint32_t done, uint32_t total) {
  if (!ui || !ui->tty) return;
  if (!ui->chapter_no && !ui->chapter_title) return;

  ui_clear_line(ui);

  ui_print_chapter_prefix(ui, 0);

  fprintf(stderr, "%s%sChapter%s", c_bold(ui), c_mag(ui), c_rst(ui));

  if (ui->chapter_no && *ui->chapter_no) {
    fprintf(stderr, " %s%s%s", c_bold(ui), ui->chapter_no, c_rst(ui));
  }
  if (ui->chapter_title && *ui->chapter_title) {
    fprintf(stderr, " %s—%s %s", c_dim(ui), c_rst(ui), ui->chapter_title);
  }

  if (total) {
    fprintf(stderr, " %s—%s %s%s%u/%u pages%s", c_dim(ui), c_rst(ui), c_bold(ui), c_grn(ui), done, total, c_rst(ui));
  } else {
    fprintf(stderr, " %s—%s %s%s%u pages%s", c_dim(ui), c_rst(ui), c_bold(ui), c_grn(ui), done, c_rst(ui));
  }
  fflush(stderr);
  ui->chapter_line_active = 1;
}

static void print_help(FILE *out) {
  fputs(
      "Usage: cml [OPTIONS] [URLS]...\n"
      "\n"
      "Download manga from MANGA Plus."
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

static void default_progress(void *user, const cml_progress_event *ev) {
  cli_ui *ui = (cli_ui *)user;
  if (!ui || !ev || !ev->stage) return;

  if (!str_eq(ev->stage, "images")) return;

  if (!str_eq(ui->title, ev->title_name) || !str_eq(ui->author, ev->title_author)) {
    ui->title = ev->title_name;
    ui->author = ev->title_author;
    ui->title_done = ev->title_done;
    ui->title_total = ev->title_total;
    ui->chapter_no = NULL;
    ui->chapter_title = NULL;
    ui->chapter_done = 0;
    ui->chapter_total = 0;
    ui->pages_done = 0;
    ui->pages_total = 0;
    ui_print_title(ui, ui->title, ui->author);
  }

  const char *ch_no = ev->chapter_no ? ev->chapter_no : NULL;
  const char *ch_title = (ev->chapter_title && ev->chapter_title[0]) ? ev->chapter_title : ev->chapter_name;
  if (!str_eq(ui->chapter_no, ch_no) || !str_eq(ui->chapter_title, ch_title) || ui->pages_total != ev->total ||
      ui->chapter_done != ev->chapter_done || ui->chapter_total != ev->chapter_total) {
    ui->chapter_no = ch_no;
    ui->chapter_title = ch_title;
    ui->chapter_done = ev->chapter_done;
    ui->chapter_total = ev->chapter_total;
    ui->pages_done = 0;
    ui->pages_total = ev->total;
  }

  ui->pages_done = ev->done;
  if (ev->total && ev->done == ev->total) {
    ui->chapters_done_total += 1;
    ui->pages_done_total += (uint64_t)ev->total;
  }
  if (ui->tty) {
    ui_render_chapter_progress(ui, ui->pages_done, ui->pages_total);
    if (ev->total && ev->done == ev->total) {
      ui_render_chapter_done(ui, ui->chapter_no, ui->chapter_title, ui->pages_total);
    }
  } else {
    if (ev->total && ev->done == ev->total) {
      ui_print_prefix(ui, 2, ui_chapter_bullet(ui), ui->chapter_done, ui->chapter_total);
      fprintf(stderr, "Chapter");
      if (ui->chapter_no && *ui->chapter_no) fprintf(stderr, " %s", ui->chapter_no);
      if (ui->chapter_title && *ui->chapter_title) fprintf(stderr, " — %s", ui->chapter_title);
      if (ui->pages_total) fprintf(stderr, " (%u pages)", ui->pages_total);
      fputc('\n', stderr);
    }
  }
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

  cli_ui ui = {
      .tty = isatty(fileno(stderr)) ? 1 : 0,
      .color = 0,
      .printed_any_title = 0,
      .chapter_line_active = 0,
      .title = NULL,
      .author = NULL,
      .title_done = 0,
      .title_total = 0,
      .chapter_no = NULL,
      .chapter_title = NULL,
      .chapter_done = 0,
      .chapter_total = 0,
      .pages_done = 0,
      .pages_total = 0,
      .chapters_done_total = 0,
      .pages_done_total = 0,
  };
  ui.color = ui.tty && ui_color_enabled();

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
      .log_fn = NULL,
      .progress_fn = default_progress,
      .user = &ui,
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
        printf("cml %s\n", CML_CLI_VERSION);
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

  if (ui.tty) {
    fprintf(stderr, "%s%scml%s %s\n", c_bold(&ui), c_cyn(&ui), c_rst(&ui), CML_CLI_VERSION);
    fprintf(stderr, "%sFetching metadata...%s\n", c_dim(&ui), c_rst(&ui));
  } else {
    fprintf(stderr, "cml %s\n", CML_CLI_VERSION);
    fprintf(stderr, "Fetching metadata...\n");
  }
  fflush(stderr);

  cml_status st = cml_run(h);
  if (ui.tty && ui.chapter_line_active) ui_end_chapter_line(&ui);
  fputc('\n', stderr);
  if (st == CML_OK) {
    fprintf(stderr, "%s%s%s%s Downloaded %u chapters (%llu pages).%s\n", c_bold(&ui), c_grn(&ui), ui_mark_ok(&ui), c_rst(&ui),
            ui.chapters_done_total, (unsigned long long)ui.pages_done_total, c_rst(&ui));
  } else {
    fprintf(stderr, "%s%s%s %s%s\n", c_bold(&ui), c_red(&ui), ui_mark_err(&ui), cml_status_string(st), c_rst(&ui));
  }
  cml_destroy(h);
  return (st == CML_OK) ? 0 : 1;
}
