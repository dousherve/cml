#include "cml_internal.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

typedef struct {
  uint8_t *data;
  size_t len;
  size_t cap;
} wbuf;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  wbuf *b = (wbuf *)userdata;
  size_t n = size * nmemb;
  if (n == 0) return 0;
  if (b->len + n > b->cap) {
    size_t next = b->cap ? b->cap : 8192;
    while (next < b->len + n) next *= 2;
    void *p = realloc(b->data, next);
    if (!p) return 0;
    b->data = (uint8_t *)p;
    b->cap = next;
  }
  memcpy(b->data + b->len, ptr, n);
  b->len += n;
  return n;
}

void cml_bytes_free(cml_bytes *b) {
  if (!b) return;
  free(b->data);
  b->data = NULL;
  b->len = 0;
}

static int is_retryable_long(long code) { return code == 429 || (code >= 500 && code <= 599); }

static void cml_sleep_usec(unsigned usec) {
  struct timespec ts;
  ts.tv_sec = (time_t)(usec / 1000000u);
  ts.tv_nsec = (long)((usec % 1000000u) * 1000u);
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }
}

cml_status cml_http_get(cml *h, const char *url, cml_bytes *out) {
  if (!h || !h->curl || !url || !out) return CML_ERR_INVALID;
  memset(out, 0, sizeof(*out));

  const int max_attempts = 4;
  for (int attempt = 1; attempt <= max_attempts; attempt++) {
    curl_easy_reset(h->curl);
    wbuf wb = {0};

    curl_easy_setopt(h->curl, CURLOPT_URL, url);
    curl_easy_setopt(h->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h->curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:72.0) Gecko/20100101 Firefox/72.0");
    curl_easy_setopt(h->curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(h->curl, CURLOPT_WRITEDATA, &wb);
    curl_easy_setopt(h->curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(h->curl, CURLOPT_TIMEOUT, 60L);

    CURLcode rc = curl_easy_perform(h->curl);
    long code = 0;
    if (rc == CURLE_OK) curl_easy_getinfo(h->curl, CURLINFO_RESPONSE_CODE, &code);

    if (rc == CURLE_OK && code >= 200 && code < 300) {
      out->data = wb.data;
      out->len = wb.len;
      return CML_OK;
    }

    free(wb.data);

    int retryable = 0;
    if (rc != CURLE_OK) {
      retryable = (rc == CURLE_COULDNT_RESOLVE_HOST || rc == CURLE_COULDNT_CONNECT || rc == CURLE_OPERATION_TIMEDOUT ||
                   rc == CURLE_RECV_ERROR || rc == CURLE_SEND_ERROR);
    } else {
      retryable = is_retryable_long(code);
    }

    if (!retryable || attempt == max_attempts) {
      cml_log(h, CML_LOG_WARN, "GET failed: %s (curl=%d http=%ld)", url, (int)rc, code);
      return CML_ERR_HTTP;
    }

    unsigned usec = (unsigned)(250000u * (1u << (unsigned)(attempt - 1)));
    cml_sleep_usec(usec);
  }

  return CML_ERR_HTTP;
}
