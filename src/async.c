#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dfs/types.h"
#include "dfs/dfs_util.h"
#include "dfs/sk_util.h"
#include "dfs/async.h"

// static pthread_mutex_t skb_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t skb_lock = PTHREAD_RWLOCK_INITIALIZER;

void *async_dfs_recv(void *arg) {
  SocketBuffer *sk_buf = *(SocketBuffer **)arg;

  // pthread_mutex_lock(&skb_mutex);
  pthread_rwlock_wrlock(&skb_lock);
#ifdef DEBUG
  fprintf(stderr, "[%s] acquired write lock on skb\n", __func__);
  fflush(stderr);
#endif
  if ((sk_buf->data = dfs_recv(sk_buf->sockfd, &(sk_buf->len_data))) == NULL) {
    return NULL;
  }

  pthread_rwlock_unlock(&skb_lock);
#ifdef DEBUG
  fprintf(stderr, "[%s] received %zd bytes\n", __func__, sk_buf->len_data);
  fprintf(stderr, "[%s] released write lock on skb\n", __func__);
  fflush(stderr);
#endif

  return NULL;
}

void *cxn_handle(void *arg) {
  DFSHandle *dfs_handle = (DFSHandle *)arg;
  SocketBuffer *sk_buf;
  DFCHeader dfc_hdr;
  pthread_t recv_tid;

  if ((sk_buf = malloc(sizeof(SocketBuffer))) == NULL) {
    fprintf(stderr, "[FATAL] out of memory\n");
    exit(EXIT_FAILURE);
  }

  sk_buf->sockfd = dfs_handle->sockfd;
  if (pthread_create(&recv_tid, NULL, async_dfs_recv, &sk_buf) < 0) {
    fprintf(stderr, "[ERROR] could not create thread\n");
    exit(EXIT_FAILURE);
  }

  pthread_join(recv_tid, NULL);

  ssize_t bytes_written;
  size_t offset = 0;
  int fd;

  while (offset < (size_t)sk_buf->len_data) {
    // extract header
    offset += strip_hdr(sk_buf->data + offset, &dfc_hdr);

    // async file write
    if ((fd = open(dfc_hdr.fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
      perror("open");

      // advance using current offset from header
      offset += dfc_hdr.offset;

      continue;
    }

    if ((bytes_written = write(fd, sk_buf->data + offset - sizeof(offset), dfc_hdr.offset)) != (ssize_t)dfc_hdr.offset) {
      fprintf(stderr, "[ERROR] incomplete/failed write: %s\n", strerror(errno));

      if (close(fd) == -1) {
        perror("close");
      }
    }

    if (close(fd) == -1) {
      perror("close");

      exit(EXIT_FAILURE);
    }
    
    // advance using current offset from header
    offset += dfc_hdr.offset;
  }

  if (close(sk_buf->sockfd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }

  free(sk_buf);

  return NULL;
}

void print_header(DFCHeader *dfc_hdr) {
  fputs("DFCHeader {\n", stderr);
  fprintf(stderr, "  cmd: %s\n", dfc_hdr->cmd);
  fprintf(stderr, "  filename: %s\n", dfc_hdr->fname);
  fprintf(stderr, "  offset: %zu\n", dfc_hdr->offset);
  fputs("}\n", stderr);
}
