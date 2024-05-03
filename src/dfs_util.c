#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dfs/dfs_util.h"

char *alloc_buf(size_t size) {
  char *buf;

  buf = (char *)malloc(size);
  if (chk_alloc_err(buf, "malloc", __func__, __LINE__ - 1) == -1) {
    return NULL;
  }

  return buf;
}

int chk_alloc_err(void *mem, const char *allocator, const char *func,
                  int line) {
  if (mem == NULL) {
    fprintf(stderr, "%s failed @%s:%d\n", allocator, func, line);
    return -1;
  }

  return 0;
}

char *realloc_buf(char *buf, size_t size) {
  char *tmp_buf;

  tmp_buf = realloc(buf, size);
  if (chk_alloc_err(tmp_buf, "realloc", __func__, __LINE__ - 1) == -1) {
    return NULL;
  }

  buf = tmp_buf;

  return buf;
}

size_t strip_hdr(char *buf, DFCHeader *dfc_hdr) {
  size_t len_hdr;

  // strncpy(dfc_hdr->cmd, buf, strlen(buf) + 1);
  strncpy(dfc_hdr->cmd, buf, sizeof(dfc_hdr->cmd) + 1);
  len_hdr = strlen(buf) + 1;

  strncpy(dfc_hdr->fname, buf + len_hdr, strlen(buf + len_hdr) + 1);
  len_hdr += strlen(buf + len_hdr) + 1;

  memcpy(&dfc_hdr->offset, buf + len_hdr, sizeof(size_t));
  len_hdr += sizeof(size_t);

  print_header(dfc_hdr);

  return len_hdr;
}

size_t strnins(char *dst, const char *src, size_t n) {
  size_t src_len, dst_len;

  src_len = strlen(src) + 1;
  dst_len = strlen(dst) + 1;

  if (n > src_len) {
    n = src_len;
  }

  char tmp[dst_len + n + 1];
  strncpy(tmp, dst, dst_len);
  strncpy(dst, src, src_len);
  strncpy(dst + src_len - 1, tmp, dst_len);

  return n;
}

void print_header(DFCHeader *dfc_hdr) {
  fputs("DFCHeader {\n", stderr);
  fprintf(stderr, "  cmd: %s\n", dfc_hdr->cmd);
  fprintf(stderr, "  filename: %s\n", dfc_hdr->fname);
  fprintf(stderr, "  offset: %zu\n", dfc_hdr->offset);
  fputs("}\n", stderr);
}
