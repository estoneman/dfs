#include <arpa/inet.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dfs/dfs.h"
#include "dfs/sk_util.h"
#include "dfs/async.h"

void usage(const char *program) {
  fprintf(stderr, "usage: %s <working-directory> <port (1024-65535)>\n",
          program);
}

int main(int argc, char *argv[]) {
  int listenfd, *connfd;
  char port[PORT_MAX + 1], dfs_dir[PATH_MAX + 1], ipstr[INET6_ADDRSTRLEN];
  struct stat st;
  struct sockaddr_in cliaddr;
  socklen_t cliaddr_len;
  pthread_t cxn_tid;

  if (argc < 3) {
    fprintf(stderr, "[ERROR] not enough arguments supplied\n");
    usage(argv[0]);

    return EXIT_FAILURE;
  }

  strncpy(dfs_dir, argv[1], PATH_MAX);
  if (stat(dfs_dir, &st) == -1) {  // path does not exist already
    if (mkdir(dfs_dir, 0744) == -1) {  // try to create
      perror("mkdir");

      return EXIT_FAILURE;
    }
  }

  strncpy(port, argv[2], PORT_MAX);
  if (!is_valid_port(port)) {
    fprintf(stderr, "[ERROR] invalid port specified\n");
    usage(argv[0]);

    return EXIT_FAILURE;
  }

  if ((listenfd = listen_sockfd(port)) == -1) {
    return EXIT_FAILURE;
  }

  if (listen(listenfd, SOMAXCONN) < 0) {
    perror("listen");

    return EXIT_FAILURE;
  }

  fprintf(stderr, "[%s] dfs listening on 0.0.0.0:%s, working directory = %s\n",
          __func__, port, dfs_dir);

  cliaddr_len = sizeof(cliaddr);

  if ((connfd = (int *)malloc(sizeof(int))) == NULL) {
    fprintf(stderr, "[ERROR] out of memory\n");

    exit(EXIT_FAILURE);
  }

  while (1) {
    fprintf(stderr, "[%s] waiting for connections... \n", __func__);
    if ((*connfd =
             accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len)) < 0) {
      perror("accept");
      continue;
    }

    get_ipstr(ipstr, (struct sockaddr *)&cliaddr);
    fprintf(stderr, "[%s] socket %d: new connection (%s:%d)\n", __func__,
            *connfd, ipstr, ntohs(cliaddr.sin_port));

    if (pthread_create(&cxn_tid, NULL, cxn_handle, connfd) != 0) {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
  }

  free(connfd);

  return EXIT_SUCCESS;
}