#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "dfs/dfs_util.h"
#include "dfs/sk_util.h"

char *dfs_recv(int sockfd, ssize_t *nb_recv) {
  char *recv_buf;
  size_t total_nb_recv, num_reallocs, bytes_alloced, realloc_sz;

  if ((recv_buf = alloc_buf(RCVCHUNK)) == NULL) {
    fprintf(stderr, "failed to allocate receive buffer (%s:%d)", __func__,
            __LINE__ - 1);

    return NULL;
  }

  set_timeout(sockfd, RCVTIMEO_SEC, RCVTIMEO_USEC);

  bytes_alloced = RCVCHUNK;

  total_nb_recv = realloc_sz = num_reallocs = 0;
  while ((*nb_recv = recv(sockfd, recv_buf + total_nb_recv, RCVCHUNK, 0)) > 0) {
    total_nb_recv += *nb_recv;

    if (total_nb_recv + RCVCHUNK >= bytes_alloced) {
      realloc_sz = bytes_alloced * 2;
      if ((recv_buf = realloc_buf(recv_buf, realloc_sz)) == NULL) {
        fprintf(stderr, "[FATAL] out of memory: attempted realloc size = %zu\n",
                realloc_sz);
        free(recv_buf);  // free old buffer

        exit(EXIT_FAILURE);
      }
    }

    bytes_alloced = realloc_sz;
    num_reallocs++;
  }

  *nb_recv = total_nb_recv;

  if (total_nb_recv == 0) {  // timeout
    // perror("recv");
    fprintf(stderr, "received 0 bytes: %s\n", strerror(errno));
    free(recv_buf);

    return NULL;
  }

  return recv_buf;
}

ssize_t dfs_send(int sockfd, char *send_buf, size_t len_send_buf) {
  ssize_t nb_sent;

  if ((nb_sent = send(sockfd, send_buf, len_send_buf, 0)) < 0) {
    perror("send");
    return -1;
  }

  return nb_sent;
}

void *get_inetaddr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void get_ipstr(char *ipstr, struct sockaddr *addr) {
  inet_ntop(addr->sa_family, get_inetaddr(addr), ipstr, INET6_ADDRSTRLEN);
}

int is_valid_port(const char *arg) {
  int port = atoi(arg);
  return (port >= 1024 && port <= 65535);
}

int listen_sockfd(const char *port) {
  struct addrinfo hints, *srv_entries, *srv_entry;
  int sockfd, addrinfo_status, enable;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((addrinfo_status = getaddrinfo(NULL, port, &hints, &srv_entries)) < 0) {
    fprintf(stderr, "[ERROR] getaddrinfo: %s\n", gai_strerror(addrinfo_status));
    return -1;
  }

  // loop through results of call to getaddrinfo
  for (srv_entry = srv_entries; srv_entry != NULL;
       srv_entry = srv_entry->ai_next) {
    // create socket through which server communication will be facililated
    if ((sockfd = socket(srv_entry->ai_family, srv_entry->ai_socktype,
                         srv_entry->ai_protocol)) < 0) {
      perror("socket");
      continue;
    }

    // convenience socket option for rapid reuse of sockets
    enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
        0) {
      perror("setsockopt");
      return -1;
    }

    // bind socket to current candidate
    if (bind(sockfd, srv_entry->ai_addr, srv_entry->ai_addrlen) < 0) {
      perror("bind");
      continue;
    }

    break;  // successfully created socket and binded to address
  }

  if (srv_entry == NULL) {
    fprintf(stderr, "[ERROR] could not bind to any address\n");
    freeaddrinfo(srv_entries);

    return -1;
  }

  freeaddrinfo(srv_entries);

  return sockfd;
}

void set_timeout(int sockfd, long tv_sec, long tv_usec) {
  struct timeval rcvtimeo;

  rcvtimeo.tv_sec = tv_sec;
  rcvtimeo.tv_usec = tv_usec;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo)) <
      0) {
    fprintf(stderr, "[%s] could not setsockopt on socket %d\n", __func__,
            sockfd);
    perror("setsockopt");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
}
