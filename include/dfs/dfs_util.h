#ifndef DFS_UTIL_H_
#define DFS_UTIL_H_

char *alloc_buf(size_t size);
int chk_alloc_err(void *, const char *, const char *, int);
char *realloc_buf(char *buf, size_t size);

#endif  // DFS_UTIL_H_
