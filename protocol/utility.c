#include "utility.h"

// print an error message and quit
void error(char const* message) {
  fprintf(stderr, "error: %s\n", message);
  exit(-1);
}

buffer* create_buffer(int size) {
  buffer* b = (buffer*)malloc(sizeof(buffer));
  b->size = size;
  b->len = 0;
  b->data = (unsigned char*)malloc(size * sizeof(unsigned char));
  return b;
}
void delete_buffer(buffer* b) {
  free(b->data);
  free(b);
}
void clear_buffer(buffer* b) {
  b->len = 0;
}
void resize_buffer(buffer* b, int size) {
  if (size < b->len) b->len = size;
  unsigned char* temp = (unsigned char*)malloc(size * sizeof(unsigned char));
  memcpy(temp, b->data, b->len * sizeof(unsigned char));
  free(b->data);
  b->data = temp;
  b->size = size;
}
void append_buffer(buffer* b, unsigned char const* str, int len) {
  if (b->len + len > b->size) resize_buffer(b, b->size * 2);
  memcpy(b->data + b->len, str, len * sizeof(unsigned char));
  b->len += len;
}
void write_buffer(buffer* b, char const* filename) {
  FILE* fp = fopen(filename, "wb");
  if (!fp) error("fopen()");
  fwrite(b->data, sizeof(unsigned char), b->len, fp);
  fclose(fp);
}
