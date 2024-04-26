#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dfs/sk_util.h"
#include "dfs/async.h"

static pthread_mutex_t skb_mutex = PTHREAD_MUTEX_INITIALIZER;

void *async_dfs_recv(void *arg) {
  SocketBuffer *sk_buf = (SocketBuffer *)arg;

  pthread_mutex_lock(&skb_mutex);
#ifdef DEBUG
  fprintf(stderr, "[%s] acquired mutex on skb\n", __func__);
  fflush(stderr);
#endif
  if ((sk_buf->data = dfs_recv(sk_buf->sockfd, &(sk_buf->len_data))) == NULL) {
    return NULL;
  }

#ifdef DEBUG
  fprintf(stderr, "[%s] received %zd bytes\n", __func__, sk_buf->len_data);
  fflush(stderr);
#endif

  pthread_mutex_unlock(&skb_mutex);
#ifdef DEBUG
  fprintf(stderr, "[%s] released mutex on skb\n", __func__);
  fflush(stderr);
#endif

  return NULL;
}

void *cxn_handle(void *arg) {
  int connfd;
  SocketBuffer sk_buf;
  pthread_t recv_pair1_tid, recv_pair2_tid;

  connfd = *(int *)arg;

  sk_buf.sockfd = connfd;
  if (pthread_create(&recv_pair1_tid, NULL, async_dfs_recv, &sk_buf) < 0) {
      fprintf(stderr, "[ERROR] could not create thread: %s:%d\n", __func__,
              __LINE__ - 1);
      close(connfd);

      return NULL;
  }

  if (pthread_create(&recv_pair2_tid, NULL, async_dfs_recv, &sk_buf) < 0) {
      fprintf(stderr, "[ERROR] could not create thread: %s:%d\n", __func__,
              __LINE__ - 1);
      close(connfd);

      return NULL;
  }

  pthread_join(recv_pair1_tid, NULL);
  pthread_join(recv_pair2_tid, NULL);

  if (sk_buf.data != NULL) {
    free(sk_buf.data);
  }

  close(connfd);

  return NULL;
}
