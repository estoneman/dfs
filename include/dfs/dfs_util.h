#ifndef DFS_UTIL_H_
#define DFS_UTIL_H_

#include "dfs/types.h"

char *alloc_buf(size_t);
int chk_alloc_err(void *, const char *, const char *, int);
size_t fill_header(DFCHeader *, char *);
char *realloc_buf(char *, size_t);
size_t strip_hdr(char *, DFCHeader *);
size_t strnins(char *, const char *, size_t);

#endif  // DFS_UTIL_H_
