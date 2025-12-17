#include <stdio.h>
#include <string.h>

#include "cml/cml.h"

static void on_progress(void *user, const cml_progress_event *ev) {
  (void)user;
  if (!ev || !ev->stage) return;
  if (strcmp(ev->stage, "images") != 0) return;

  if (ev->title_name && ev->chapter_no && ev->total) {
    fprintf(stderr, "\r%s — Chapter %s: %u/%u pages", ev->title_name, ev->chapter_no, ev->done, ev->total);
    fflush(stderr);
    if (ev->done == ev->total) fputc('\n', stderr);
  }
}

int main(void) {
  cml_config cfg = {
      .out_dir = "cml_downloads",
      .output = CML_OUTPUT_CBZ,
      .quality = CML_QUALITY_SUPER_HIGH,
      .split = false,
      .min_chapter = 0,
      .max_chapter = 0,
      .last_only = false,
      .include_chapter_title = true,
      .chapter_subdir = false,
      .log_fn = NULL,
      .progress_fn = on_progress,
      .user = NULL,
  };

  cml *h = cml_create(&cfg);
  if (!h) {
    fprintf(stderr, "cml: failed to initialize\n");
    return 1;
  }

  const uint32_t chapter_id = 1013146;
  cml_status st = cml_add_chapter_id(h, chapter_id);
  if (st != CML_OK) {
    fprintf(stderr, "cml: %s\n", cml_status_string(st));
    cml_destroy(h);
    return 1;
  }

  st = cml_run(h);
  if (st != CML_OK) fprintf(stderr, "cml: %s\n", cml_status_string(st));

  cml_destroy(h);
  return (st == CML_OK) ? 0 : 1;
}
