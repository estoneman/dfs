#ifndef SK_UTIL_H_
#define SK_UTIL_H_

#include <netinet/in.h>

#define RCVTIMEO_USEC 0
#define RCVTIMEO_SEC 5
#define RCVCHUNK 4096

char *alloc_buf(size_t);
char *dfs_recv(int, ssize_t *);
ssize_t dfs_send(int, char *, size_t);
void *get_inetaddr(struct sockaddr *);
void get_ipstr(char *ipstr, struct sockaddr *addr);
int listen_sockfd(const char *);
int is_valid_port(const char *);
void set_timeout(int, long, long);

#endif  // SK_UTIL_H_
