#ifndef ASYNC_H_
#define ASYNC_H_

void cxn_handle(int, char *);
void get_handle(GetOperation *);
void put_handle(FileBuffer *);

void print_fbuf(FileBuffer *);

#endif  // ASYNC_H_
