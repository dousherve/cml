#include "cml_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int cml_exists(const char *path) {
  if (!path) return 0;
  struct stat st;
  return stat(path, &st) == 0;
}

static cml_status mkdir_one(const char *path) {
  if (mkdir(path, 0755) == 0) return CML_OK;
  if (errno == EEXIST) return CML_OK;
  return CML_ERR_IO;
}

cml_status cml_mkdir_p(const char *path) {
  if (!path || !*path) return CML_ERR_INVALID;
  char *tmp = strdup(path);
  if (!tmp) return CML_ERR_OOM;
  size_t n = strlen(tmp);
  while (n > 0 && tmp[n - 1] == '/') tmp[--n] = '\0';
  if (n == 0) {
    free(tmp);
    return CML_ERR_INVALID;
  }
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      cml_status st = mkdir_one(tmp);
      *p = '/';
      if (st != CML_OK) {
        free(tmp);
        return st;
      }
    }
  }
  cml_status st = mkdir_one(tmp);
  free(tmp);
  return st;
}

static cml_status write_all(int fd, const uint8_t *data, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t w = write(fd, data + off, len - off);
    if (w < 0) {
      if (errno == EINTR) continue;
      return CML_ERR_IO;
    }
    off += (size_t)w;
  }
  return CML_OK;
}

cml_status cml_rename_overwrite(const char *src, const char *dst) {
  if (!src || !dst) return CML_ERR_INVALID;
  if (rename(src, dst) == 0) return CML_OK;
  if (errno == EEXIST) {
    unlink(dst);
    if (rename(src, dst) == 0) return CML_OK;
  }
  return CML_ERR_IO;
}

cml_status cml_write_file_atomic(const char *path, const uint8_t *data, size_t len) {
  if (!path || !data) return CML_ERR_INVALID;
  size_t n = strlen(path) + 8;
  char *tmp = (char *)malloc(n);
  if (!tmp) return CML_ERR_OOM;
  snprintf(tmp, n, "%s.tmp", path);

  int fd = open(tmp, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    free(tmp);
    return CML_ERR_IO;
  }
  cml_status st = write_all(fd, data, len);
  if (st == CML_OK && fsync(fd) != 0) st = CML_ERR_IO;
  if (close(fd) != 0) st = CML_ERR_IO;
  if (st != CML_OK) {
    unlink(tmp);
    free(tmp);
    return st;
  }
  st = cml_rename_overwrite(tmp, path);
  if (st != CML_OK) unlink(tmp);
  free(tmp);
  return st;
}

