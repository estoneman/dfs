#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dfs/types.h"
#include "dfs/dfs_util.h"
#include "dfs/sk_util.h"
#include "dfs/async.h"
#include "dfs/dfs.h"

void usage(const char *program) {
  fprintf(stderr, "usage: %s <working-directory> <port (1024-65535)>\n",
          program);
}

void sigchld_handler(int s __attribute__((unused))) {
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0) {
  }  // reap dead child processes

  errno = saved_errno;
}

int main(int argc, char *argv[]) {
  int listenfd;
  char port[PORT_MAX + 1], dfs_dir[PATH_MAX + 1], ipstr[INET6_ADDRSTRLEN];
  struct stat st;
  struct sockaddr_in cliaddr;
  socklen_t cliaddr_len;
  struct sigaction sa;

  if (argc < 3) {
    fprintf(stderr, "[ERROR] not enough arguments supplied\n");
    usage(argv[0]);

    return EXIT_FAILURE;
  }

  strncpy(dfs_dir, argv[1], PATH_MAX);
  if (stat(dfs_dir, &st) == -1) {      // path does not exist already
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

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  fprintf(stderr,
          "[%s] dfs listening on %d -> 0.0.0.0:%s, working directory = %s\n",
          __func__, listenfd, port, dfs_dir);

  cliaddr_len = sizeof(cliaddr);

  int connfd;
  pid_t pid_chld;

  if (dfs_dir[strlen(dfs_dir) - 1] != '/')
    strncat(dfs_dir, "/", sizeof(char) + 1);

  while (1) {
    fprintf(stderr, "[%s:%d] waiting for connections... \n", __func__, getpid());

    if ((connfd =
             accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len)) < 0) {
      perror("accept");
      continue;
    }

    get_ipstr(ipstr, (struct sockaddr *)&cliaddr);
    fprintf(stderr, "[%s] socket %d: new connection (%s:%d)\n", __func__,
            connfd, ipstr, ntohs(cliaddr.sin_port));

    if ((pid_chld = fork()) == -1) {
      perror("fork");
      close(connfd);
      continue;
    } else if (pid_chld == 0) {  // child process
      close(listenfd);

      cxn_handle(connfd, dfs_dir);

      exit(EXIT_SUCCESS);
    } else {  // parent process
      close(connfd);
    }
  }

  return EXIT_SUCCESS;
}
