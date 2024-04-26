#ifndef ASYNC_H_
#define ASYNC_H_

typedef struct {
  int sockfd;
  char *data;
  ssize_t len_data;
} SocketBuffer;

void *cxn_handle(void *);

#endif  // ASYNC_H_
