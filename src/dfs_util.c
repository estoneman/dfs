#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

char *read_file(const char *fpath, size_t *nb_read) {
  char *out_buf;
  FILE *fp;
  struct stat st;

  if ((fp = fopen(fpath, "rb")) == NULL) {
    // server error
    return NULL;
  }

  if (stat(fpath, &st) < 0) {
    // server error
    fclose(fp);

    return NULL;
  }

  out_buf = alloc_buf(st.st_size);
  chk_alloc_err(out_buf, "malloc", __func__, __LINE__ - 1);

  if ((*nb_read = fread(out_buf, 1, st.st_size, fp)) < (size_t)st.st_size) {
    fclose(fp);
    free(out_buf);

    return NULL;
  }

  fclose(fp);

  return out_buf;
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

  strncpy(dfc_hdr->cmd, buf, strlen(buf));
  len_hdr = strlen(buf);

  while (buf[len_hdr++] == '\0') {}
  len_hdr--;

  strncpy(dfc_hdr->fname, buf + len_hdr, strlen(buf + len_hdr));
  len_hdr += strlen(buf + len_hdr);

  while (buf[len_hdr++] == '\0') {}
  len_hdr--;

  memcpy(&dfc_hdr->chunk_offset, buf + len_hdr, sizeof(size_t));
  len_hdr += sizeof(size_t);

  memcpy(&dfc_hdr->file_offset, buf + len_hdr, sizeof(size_t));
  len_hdr += sizeof(size_t);

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
  fprintf(stderr, "  chunk offset: %zu\n", dfc_hdr->chunk_offset);
  fprintf(stderr, "  file offset: %zu\n", dfc_hdr->file_offset);
  fputs("}\n", stderr);
}
