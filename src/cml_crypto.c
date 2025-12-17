#include "cml_internal.h"

#include <stdlib.h>
#include <string.h>

static int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

cml_status cml_decrypt_xor_hex(uint8_t *data, size_t data_len, const char *hex_key) {
  if (!data || !hex_key) return CML_ERR_INVALID;
  size_t hex_len = strlen(hex_key);
  if (hex_len == 0 || (hex_len % 2) != 0) return CML_ERR_INVALID;
  size_t key_len = hex_len / 2;
  uint8_t *key = (uint8_t *)malloc(key_len);
  if (!key) return CML_ERR_OOM;

  for (size_t i = 0; i < key_len; i++) {
    int hi = hex_val(hex_key[i * 2]);
    int lo = hex_val(hex_key[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      free(key);
      return CML_ERR_INVALID;
    }
    key[i] = (uint8_t)((hi << 4) | lo);
  }

  for (size_t i = 0; i < data_len; i++) data[i] ^= key[i % key_len];
  free(key);
  return CML_OK;
}

