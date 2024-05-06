#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dfs/types.h"
#include "dfs/dfs_util.h"
#include "dfs/sk_util.h"
#include "dfs/async.h"

#define MAX_FILES 100 
#define MAX_ENTRY 256

void *async_dfs_recv(void *arg) {
  SocketBuffer *sk_buf = (SocketBuffer *)arg;

  if ((sk_buf->data = dfs_recv(sk_buf->sockfd, &(sk_buf->len_data))) == NULL) {
    perror("recv");
    exit(EXIT_FAILURE);
  }

  return NULL;
}

void *async_dfs_send(void *arg) {
  SocketBuffer *sk_buf = (SocketBuffer *)arg;
  ssize_t bytes_sent;

  if ((bytes_sent = dfs_send(sk_buf->sockfd, sk_buf->data,
                             sk_buf->len_data)) != sk_buf->len_data) {
    fprintf(stderr, "[ERROR] incomplete send\n");
  }

#ifdef DEBUG
  fprintf(stderr, "[%s] sent %zd bytes\n", __func__, bytes_sent);
  fflush(stderr);
#endif

  return NULL;
}

void cxn_handle(int connfd, char *dfs_dir) {
  SocketBuffer hdr_sk_buf;
  SocketBuffer data_sk_buf;
  FileBuffer f_buf;
  GetOperation get_op;
  DFCHeader dfc_hdr;

  char fname[PATH_MAX + 1];

  hdr_sk_buf.sockfd = connfd;
  if ((hdr_sk_buf.data = dfs_recv_hdr(connfd, &(hdr_sk_buf.len_data))) == NULL) {
    perror("recv");
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "received %zd bytes over sfd=%d\n", hdr_sk_buf.len_data, connfd);

  // extract header
  memcpy(&dfc_hdr, hdr_sk_buf.data, hdr_sk_buf.len_data);
  fprintf(stderr, "successfully parsed %zu bytes from data\n", hdr_sk_buf.len_data);
  print_header(&dfc_hdr);

  strncpy(fname, basename(dfc_hdr.fname), PATH_MAX);
  strnins(fname, dfs_dir, PATH_MAX);

  // decide if put, get, or list
  if (strncmp(dfc_hdr.cmd, "put", sizeof(dfc_hdr.cmd)) == 0) {  // put command
    // receive file data
    if ((data_sk_buf.data = dfs_recv(connfd, &(data_sk_buf.len_data))) == NULL) {
      exit(EXIT_FAILURE);
    }

    f_buf.len_data = dfc_hdr.file_offset + sizeof(size_t);

    if ((f_buf.data = alloc_buf(f_buf.len_data)) == NULL) {
      fprintf(stderr, "[FATAL] out of memory\n");
      exit(EXIT_FAILURE);
    }

    strncpy(f_buf.fname, fname, sizeof(f_buf.fname));
    memcpy(f_buf.data, &dfc_hdr.chunk_offset, sizeof(size_t));
    memcpy(f_buf.data + sizeof(size_t), data_sk_buf.data, f_buf.len_data);

    put_handle(&f_buf);
  } else if (strncmp(dfc_hdr.cmd, "get", sizeof(dfc_hdr.cmd)) == 0) {  // get command
    strncpy(f_buf.fname, fname, sizeof(f_buf.fname));
    f_buf.len_data = 0;

    get_op.sockfd = connfd;
    get_op.f_buf = &f_buf;
  
    get_handle(&get_op);
  } else if (strncmp(dfc_hdr.cmd, "list", sizeof(dfc_hdr.cmd)) == 0) {  // list command
    DIR *dir;
    struct dirent *entry;
    char *snd_buf;
    size_t entry_len, len_snd_buf;
    ssize_t bytes_sent;

    if ((dir = opendir(dfs_dir)) == NULL) {
      fprintf(stderr, "[%s:%d] failed to open %s: %s\n", __func__, getpid(), dfs_dir, strerror(errno));
    }

    if ((snd_buf = alloc_buf(MAX_FILES * MAX_ENTRY)) == NULL) {
      fprintf(stderr, "[FATAL] out of memory\n");
      exit(EXIT_FAILURE);
    }

    len_snd_buf = 0;
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }

      entry_len = strlen(entry->d_name) + 1;
      strncpy(snd_buf + len_snd_buf, entry->d_name, entry_len);
      printf("%s\n", snd_buf + len_snd_buf);
      len_snd_buf += entry_len;
    }

    // ssize_t dfs_send(int sockfd, char *send_buf, size_t len_send_buf) {
    if ((bytes_sent = dfs_send(connfd, snd_buf, len_snd_buf)) != (ssize_t)len_snd_buf) {
      fprintf(stderr, "[%s:%d] failed to send %s: %s\n", __func__, getpid(), dfs_dir, strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (closedir(dir) == -1) {
      fprintf(stderr, "[%s:%d] failed to close %s: %s\n", __func__, getpid(), dfs_dir, strerror(errno));
      exit(EXIT_FAILURE);
    }

    free(snd_buf);
  } else {  // invalid command
    fputs("invalid response handler: unimplemented\n", stderr);
    exit(EXIT_FAILURE);
  }

  free(hdr_sk_buf.data);
  free(data_sk_buf.data);
  free(f_buf.data);

  exit(EXIT_SUCCESS);
}

// void *get_handle(void *arg) {
void get_handle(GetOperation *get_op) {
  FileBuffer *f_buf = get_op->f_buf;
  int fd;
  ssize_t bytes_read, bytes_sent;
  struct stat st;

  // read from file
  if ((fd = open(f_buf->fname, O_RDONLY)) == -1) {
    fprintf(stderr, "[%s:%d] failed to open %s: %s\n", __func__, getpid(),
            f_buf->fname, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (fstat(fd, &st) == -1) {
    fprintf(stderr, "[%s:%d] failed to stat %s: %s\n", __func__, getpid(), f_buf->fname, strerror(errno));
    exit(EXIT_FAILURE);
  }

  f_buf->len_data = st.st_size;

  if ((f_buf->data = alloc_buf(f_buf->len_data)) == NULL) {
    fprintf(stderr, "[FATAL] out of memory\n");
    exit(EXIT_FAILURE);
  }

  if ((bytes_read = read(fd, f_buf->data, f_buf->len_data)) != (ssize_t)f_buf->len_data) {
    fprintf(stderr, "[%s:%d] failed to read fd=%d: %s\n", __func__, getpid(), fd, strerror(errno));
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "[%s:%d] read %zd bytes from fd=%d\n", __func__, getpid(), bytes_read, fd);

  // send file contents
  if ((bytes_sent = write(get_op->sockfd, f_buf->data, f_buf->len_data)) == -1) {
    fprintf(stderr, "[%s:%d] failed to send over sfd=%d: %s\n", __func__, getpid(), get_op->sockfd, strerror(errno));
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "[%s:%d] sent %zd bytes over sfd=%d\n", __func__, getpid(), bytes_sent, get_op->sockfd);

  if (close(fd) == -1) {
    fprintf(stderr, "[%s:%d] failed to close fd=%d: %s\n", __func__, getpid(), fd, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void put_handle(FileBuffer *f_buf) {
  int fd;
  ssize_t bytes_written;

  if ((fd = open(f_buf->fname, O_CREAT | O_TRUNC | O_WRONLY, S_IWUSR | S_IRUSR)) == -1) {
    fprintf(stderr, "[%s:%d] failed to open %s: %s\n", __func__, getpid(),
            f_buf->fname, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if ((bytes_written = write(fd, f_buf->data, f_buf->len_data)) == -1) {
    fprintf(stderr, "[%s:%d] failed to completely write to %s: %s\n", __func__, getpid(),
            f_buf->fname, strerror(errno));
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "[%s:%d] wrote %zd bytes to %s\n", __func__, getpid(), bytes_written, f_buf->fname);

  if (close(fd) == -1) {
    fprintf(stderr, "[%s:%d] failed to close fd=%d: %s\n", __func__, getpid(),
            fd, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void print_fbuf(FileBuffer *f_buf) {
  fputs("FileBuffer {\n", stderr);
  fprintf(stderr, "  fname: %s\n", f_buf->fname);
  fprintf(stderr, "  data: <omitted>\n");
  fprintf(stderr, "  len_data: %zu\n", f_buf->len_data);
  fputs("}\n", stderr);
}
