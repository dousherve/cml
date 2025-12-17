#include "cml_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *API_BASE = "https://jumpg-webapi.tokyo-cdn.com";

static const char *quality_param(cml_quality q) {
  switch (q) {
    case CML_QUALITY_SUPER_HIGH:
      return "super_high";
    case CML_QUALITY_HIGH:
      return "high";
    case CML_QUALITY_LOW:
      return "low";
    default:
      return "super_high";
  }
}

static cml_status api_get(cml *h, const char *path_and_query, cml_bytes *out) {
  size_t n = strlen(API_BASE) + strlen(path_and_query) + 1;
  char *url = (char *)malloc(n);
  if (!url) return CML_ERR_OOM;
  snprintf(url, n, "%s%s", API_BASE, path_and_query);
  cml_status st = cml_http_get(h, url, out);
  free(url);
  return st;
}

cml_status cml_api_get_manga_viewer(cml *h, uint32_t chapter_id, cml_manga_viewer *out) {
  if (!h || !out || chapter_id == 0) return CML_ERR_INVALID;
  char query[256];
  snprintf(query, sizeof(query), "/api/manga_viewer?chapter_id=%u&split=%s&img_quality=%s", chapter_id,
           h->cfg.split ? "yes" : "no", quality_param(h->cfg.quality));

  cml_bytes resp = {0};
  cml_status st = api_get(h, query, &resp);
  if (st != CML_OK) return st;
  st = cml_proto_parse_manga_viewer(resp.data, resp.len, out);
  cml_bytes_free(&resp);
  return st;
}

cml_status cml_api_get_title_detail(cml *h, uint32_t title_id, cml_title_detail *out) {
  if (!h || !out || title_id == 0) return CML_ERR_INVALID;
  char query[128];
  snprintf(query, sizeof(query), "/api/title_detailV3?title_id=%u", title_id);
  cml_bytes resp = {0};
  cml_status st = api_get(h, query, &resp);
  if (st != CML_OK) return st;
  st = cml_proto_parse_title_detail(resp.data, resp.len, out);
  cml_bytes_free(&resp);
  return st;
}

