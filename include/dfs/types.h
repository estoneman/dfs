#ifndef TYPES_H_
#define TYPES_H_

#include <limits.h>

#define SZ_CMD_MAX 8

typedef struct {
  int sockfd;
  char *dfs_dir;
} DFSHandle;

typedef struct {
  char cmd[SZ_CMD_MAX];
  char fname[PATH_MAX + 1];
  size_t offset;
} DFCHeader;
#define DFC_HDRSZ sizeof(DFCHeader)

typedef struct {
  int fd;
  char *data;
  ssize_t len_data;
  pthread_mutex_t mutex;
} FileBuffer;

typedef struct {
  int sockfd;
  char *data;
  ssize_t len_data;
  pthread_mutex_t mutex;
} SocketBuffer;

#endif  // TYPES_H_
