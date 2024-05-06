#ifndef TYPES_H_
#define TYPES_H_

#include <limits.h>

#define SZ_CMD_MAX 8

typedef struct {
  char cmd[SZ_CMD_MAX + 1];
  char fname[PATH_MAX + 1];
  size_t chunk_offset;
  size_t file_offset;
} DFCHeader;

typedef struct {
  char fname[PATH_MAX + 1];
  char *data;
  size_t len_data;
} FileBuffer;

typedef struct {
  int sockfd;
  char *data;
  ssize_t len_data;
} SocketBuffer;

typedef struct {
  int sockfd;
  FileBuffer *f_buf;
} GetOperation;

#endif  // TYPES_H_
