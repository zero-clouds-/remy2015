#ifndef UTILITY_H
#define UTILITY_H

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

void error(char const* message);

typedef struct buffer_t {
  int size, len;
  unsigned char* data;
} buffer;

buffer* create_buffer(int size);
void delete_buffer(buffer* b);
void clear_buffer(buffer* b);
void resize_buffer(buffer* b, int size);
void append_buffer(buffer* b, unsigned char const* str, int len);
void write_buffer(buffer* b, char const* filename);

#endif
