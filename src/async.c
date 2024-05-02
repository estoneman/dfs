#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
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

#define SZ_THREAD_POOL 10
#define INIT_NBUFS SZ_THREAD_POOL

void *async_dfs_recv(void *arg) {
  SocketBuffer *sk_buf = *(SocketBuffer **)arg;

  pthread_mutex_lock(&sk_buf->mutex);
  if ((sk_buf->data = dfs_recv(sk_buf->sockfd, &(sk_buf->len_data))) == NULL) {
    return NULL;
  }

#ifdef DEBUG
  fprintf(stderr, "[%s] received %zd bytes\n", __func__, sk_buf->len_data);
  fflush(stderr);
#endif

  pthread_mutex_unlock(&sk_buf->mutex);

  return NULL;
}

void *async_dfs_write(void *arg) {
  FileBuffer *f_buf = *(FileBuffer **)arg;
  ssize_t bytes_written;

  pthread_mutex_lock(&f_buf->mutex);

  if ((bytes_written = write(f_buf->fd, f_buf->data, f_buf->len_data)) != f_buf->len_data) {
    if (errno == EFAULT) {
      fprintf(stderr, "[ERROR] incomplete/failed write: %s\n", strerror(errno));
    } else {
      perror("write");
    }

    if (close(f_buf->fd) == -1) {
      perror("close");
    }
  }

#ifdef DEBUG
  fprintf(stderr, "[%s] wrote %zd bytes (fd=%d)\n", __func__, bytes_written, f_buf->fd);
#endif

  pthread_mutex_unlock(&f_buf->mutex);

  return NULL;
}

void *cxn_handle(void *arg) {
  DFSHandle *dfs_handle = (DFSHandle *)arg;
  SocketBuffer *sk_buf;
  DFCHeader dfc_hdr;
  FileBuffer **f_bufs;

  pthread_t recv_tid, write_tids[SZ_THREAD_POOL];
  pthread_attr_t write_tid_attrs[SZ_THREAD_POOL];

  size_t data_offset, cur_write_tid, ran_threads[SZ_THREAD_POOL];
  size_t n_fbufs, total_fbufs;

  char fname[PATH_MAX + 1];

  int detach_state;

  if ((sk_buf = malloc(sizeof(SocketBuffer))) == NULL) {
    fprintf(stderr, "[FATAL] out of memory\n");
    exit(EXIT_FAILURE);
  }

  pthread_mutex_init(&sk_buf->mutex, NULL);
  sk_buf->sockfd = dfs_handle->sockfd;
  if (pthread_create(&recv_tid, NULL, async_dfs_recv, &sk_buf) < 0) {
    fprintf(stderr, "[ERROR] could not create thread\n");
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < SZ_THREAD_POOL; ++i) {
    pthread_attr_init(&write_tid_attrs[i]);
    pthread_attr_setdetachstate(&write_tid_attrs[i], PTHREAD_CREATE_JOINABLE);
  }

  if ((f_bufs = malloc(sizeof(FileBuffer *) * INIT_NBUFS)) == NULL) {
    fprintf(stderr, "[FATAL] out of memory\n");
    exit(EXIT_FAILURE);
  }

  cur_write_tid = 0;
  data_offset = 0;
  n_fbufs = 0;
  total_fbufs = INIT_NBUFS;

  pthread_join(recv_tid, NULL);

  while (data_offset < (size_t)sk_buf->len_data) {
    // extract header
    data_offset += strip_hdr(sk_buf->data + data_offset, &dfc_hdr);

    if ((f_bufs[n_fbufs] = malloc(sizeof(FileBuffer))) == NULL) {
      fprintf(stderr, "[FATAL] out of memory\n");
      exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&f_bufs[n_fbufs]->mutex, NULL);

    strncpy(fname, basename(dfc_hdr.fname), PATH_MAX);
    strnins(fname, dfs_handle->dfs_dir, PATH_MAX);

    // update f_buf
    pthread_mutex_lock(&f_bufs[n_fbufs]->mutex);

    f_bufs[n_fbufs]->fname = fname;
    f_bufs[n_fbufs]->data = sk_buf->data + data_offset;  // include local offset in file data
    f_bufs[n_fbufs]->len_data = dfc_hdr.offset;

    if ((f_bufs[n_fbufs]->fd = open(f_bufs[n_fbufs]->fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
      perror("open");

      return NULL;
    }

    // advance using current offset from header
    data_offset += dfc_hdr.offset;
    pthread_mutex_unlock(&f_bufs[n_fbufs]->mutex);

    // async file write
    if (pthread_create(&write_tids[cur_write_tid], &write_tid_attrs[cur_write_tid], async_dfs_write, &f_bufs[n_fbufs]) < 0) {
      fprintf(stderr, "[ERROR] could not create thread\n");
      exit(EXIT_FAILURE);
    }

    n_fbufs++;
    ran_threads[(cur_write_tid++) % SZ_THREAD_POOL] = 1;

    if (n_fbufs >= total_fbufs) {
      total_fbufs *= 2;
      
      if ((f_bufs = realloc(f_bufs, total_fbufs * sizeof(FileBuffer *))) == NULL) {
        fprintf(stderr, "[FATAL] out of memory\n");
        free(f_bufs);
        exit(EXIT_FAILURE);
      }
    }

    // if surpassed thread pool depth, check for joinable threads
    if (cur_write_tid >= SZ_THREAD_POOL) {
      for (size_t i = 0; i < SZ_THREAD_POOL; ++i) {
        pthread_attr_getdetachstate(&write_tid_attrs[i], &detach_state); 
        if (detach_state == PTHREAD_CREATE_JOINABLE) {
          fprintf(stderr, "[INFO] joining completed thread %zu\n", i);
          pthread_join(write_tids[i], NULL);
          cur_write_tid = i;
          ran_threads[i] = 0;
          break;  // only need one back
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

  for (size_t i = 0; i < n_fbufs; ++i) {
    if (close(f_bufs[i]->fd) == -1) {
      perror("close");

      exit(EXIT_FAILURE);
    }

    pthread_mutex_destroy(&f_bufs[i]->mutex);
    if (f_bufs[i] != NULL) {
      free(f_bufs[i]);
    }
  }
  if (f_bufs != NULL) {
    free(f_bufs);
  }

  pthread_mutex_destroy(&sk_buf->mutex);
  if (sk_buf->data != NULL) {
    free(sk_buf->data);
  }

  if (close(sk_buf->sockfd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }

  if (sk_buf != NULL) {
    free(sk_buf);
  }

  return NULL;
}

void print_header(DFCHeader *dfc_hdr) {
  fputs("DFCHeader {\n", stderr);
  fprintf(stderr, "  cmd: %s\n", dfc_hdr->cmd);
  fprintf(stderr, "  filename: %s\n", dfc_hdr->fname);
  fprintf(stderr, "  offset: %zu\n", dfc_hdr->offset);
  fputs("}\n", stderr);
}

void print_fbuf(FileBuffer *f_buf) {
  fputs("FileBuffer {\n", stderr);
  fprintf(stderr, "  fname: %s\n", f_buf->fname);
  fprintf(stderr, "  fd: %d\n", f_buf->fd);
  fprintf(stderr, "  data: <omitted>\n");
  fprintf(stderr, "  len_data: %zu\n", f_buf->len_data);
  fprintf(stderr, "  mutex: <omitted>\n");
  fputs("}\n", stderr);
}
