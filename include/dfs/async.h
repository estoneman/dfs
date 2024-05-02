#ifndef ASYNC_H_
#define ASYNC_H_

void *async_dfs_write(void *);
void *async_dfs_recv(void *);
void *cxn_handle(void *);

void print_header(DFCHeader *);

#endif  // ASYNC_H_
