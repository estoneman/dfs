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

#define SZ_THREAD_POOL 5

void *async_dfs_recv(void *arg) {
  SocketBuffer *sk_buf = *(SocketBuffer **)arg;

  pthread_mutex_lock(&sk_buf->mutex);
#ifdef DEBUG
  fprintf(stderr, "[%s] acquired write lock on skb\n", __func__);
  fflush(stderr);
#endif
  if ((sk_buf->data = dfs_recv(sk_buf->sockfd, &(sk_buf->len_data))) == NULL) {
    return NULL;
  }

  pthread_mutex_unlock(&sk_buf->mutex);
#ifdef DEBUG
  fprintf(stderr, "[%s] received %zd bytes\n", __func__, sk_buf->len_data);
  fprintf(stderr, "[%s] released write lock on skb\n", __func__);
  fflush(stderr);
#endif

  return NULL;
}

void *async_dfs_write(void *arg) {
  FileBuffer *f_buf = *(FileBuffer **)arg;
  ssize_t bytes_written;

  pthread_mutex_lock(&f_buf->mutex);
  if ((bytes_written = write(f_buf->fd, f_buf->data, f_buf->len_data)) != f_buf->len_data) {
    if (errno == EFAULT) {
      fprintf(stderr, "[ERROR] incomplete/failed write: %s\n", strerror(errno));
    }

    if (close(f_buf->fd) == -1) {
      perror("close");
    }
  }
  pthread_mutex_unlock(&f_buf->mutex);

  if (close(f_buf->fd) == -1) {
    perror("close");

    exit(EXIT_FAILURE);
  }

  return NULL;
}

void *cxn_handle(void *arg) {
  DFSHandle *dfs_handle = (DFSHandle *)arg;
  SocketBuffer *sk_buf;
  FileBuffer *f_buf;
  DFCHeader dfc_hdr;
  pthread_t recv_tid, write_tids[SZ_THREAD_POOL];
  pthread_attr_t write_tid_attrs[SZ_THREAD_POOL];
  size_t data_offset, cur_write_tid, ran_threads[SZ_THREAD_POOL];
  int fd;

  if ((sk_buf = malloc(sizeof(SocketBuffer))) == NULL) {
    fprintf(stderr, "[FATAL] out of memory\n");
    exit(EXIT_FAILURE);
  }

  if ((f_buf = malloc(sizeof(FileBuffer))) == NULL) {
    fprintf(stderr, "[FATAL] out of memory\n");
    exit(EXIT_FAILURE);
  }

  pthread_mutex_init(&sk_buf->mutex, NULL);
  sk_buf->sockfd = dfs_handle->sockfd;
  if (pthread_create(&recv_tid, NULL, async_dfs_recv, &sk_buf) < 0) {
    fprintf(stderr, "[ERROR] could not create thread\n");
    exit(EXIT_FAILURE);
  }

  pthread_join(recv_tid, NULL);

  // don't need, let client know recepit successful by closing connection
  if (close(sk_buf->sockfd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }

  data_offset = 0;
  pthread_mutex_init(&f_buf->mutex, NULL);
  cur_write_tid = 0;
  
  for (size_t i = 0; i < SZ_THREAD_POOL; ++i) {
    pthread_attr_init(&write_tid_attrs[i]);
    pthread_attr_setdetachstate(&write_tid_attrs[i], PTHREAD_CREATE_JOINABLE);
  }

  while (data_offset < (size_t)sk_buf->len_data) {
    // extract header
    data_offset += strip_hdr(sk_buf->data + data_offset, &dfc_hdr);

    strnins(dfc_hdr.fname, dfs_handle->dfs_dir, PATH_MAX);
    if ((fd = open(dfc_hdr.fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
      perror("open");

      // advance using current offset from header
      data_offset += dfc_hdr.offset;

      continue;
    }

    pthread_mutex_lock(&f_buf->mutex);

    // update f_buf
    f_buf->fd = fd;
    f_buf->data = sk_buf->data + data_offset;  // include local offset in file data
    f_buf->len_data = dfc_hdr.offset;

    // advance using current offset from header
    data_offset += dfc_hdr.offset;
    pthread_mutex_unlock(&f_buf->mutex);

    // async file write
    if (pthread_create(&write_tids[cur_write_tid], &write_tid_attrs[cur_write_tid], async_dfs_write, &f_buf) < 0) {
      fprintf(stderr, "[ERROR] could not create thread\n");
      exit(EXIT_FAILURE);
    }

    ran_threads[(cur_write_tid++) % SZ_THREAD_POOL] = 1;

    // if ran out of threads (maybe)
    int detach_state;
    if (cur_write_tid >= SZ_THREAD_POOL) {
      for (size_t i = 0; i < SZ_THREAD_POOL; ++i) {
        pthread_attr_getdetachstate(&write_tid_attrs[i], &detach_state); 
        if (detach_state == PTHREAD_CREATE_JOINABLE) {
          fprintf(stderr, "[INFO] joining completed thread %zu\n", i);
          pthread_join(write_tids[i], NULL);
          cur_write_tid = i;
          break;
          // ran_threads[i] = 0; -- this is causing some threads to not be
          // joined
        }
      }
    }
  }

  for (size_t i = 0; i < SZ_THREAD_POOL; ++i) {
    if (ran_threads[i]) {
      pthread_join(write_tids[i], NULL);
    }
    pthread_attr_destroy(&write_tid_attrs[i]);
  }

  pthread_mutex_destroy(&sk_buf->mutex);
  pthread_mutex_destroy(&f_buf->mutex);

  free(sk_buf->data);
  free(sk_buf);
  free(f_buf);

  return NULL;
}

void print_header(DFCHeader *dfc_hdr) {
  fputs("DFCHeader {\n", stderr);
  fprintf(stderr, "  cmd: %s\n", dfc_hdr->cmd);
  fprintf(stderr, "  filename: %s\n", dfc_hdr->fname);
  fprintf(stderr, "  offset: %zu\n", dfc_hdr->offset);
  fputs("}\n", stderr);
}
