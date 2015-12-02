#include "utility.h"

// print an error message and quit
void error(char const* message) {
  fprintf(stderr, "error: %s\n", message);
  exit(-1);
}

/*
* create a buffer of size "size" and return the pointer
*/
buffer* create_buffer(int size) {
  buffer* b = (buffer*)malloc(sizeof(buffer));
  b->size = size;
  b->len = 0;
  b->data = (unsigned char*)malloc(size * sizeof(unsigned char));
  return b;
}

/*
* Delete buffer by freeing data in buffer and then the pointer itself.
*/
void delete_buffer(buffer* b) {
  free(b->data);
  free(b);
}

/*
* clear out the buffer by setting its length to 0 (will just be rewritten over)
*/
void clear_buffer(buffer* b) {
  b->len = 0;
}

/*
* resize the buffer to its new length (size)
*/
void resize_buffer(buffer* b, int size) {
  if (size < b->len) b->len = size;
  unsigned char* temp = (unsigned char*)malloc(size * sizeof(unsigned char));
  memcpy(temp, b->data, b->len * sizeof(unsigned char));
  free(b->data);
  b->data = temp;
  b->size = size;
}

/*
* Add the first len bytes of data in str to the end of the buffer.
*/
void append_buffer(buffer* b, unsigned char const* str, int len) {
  if (b->len + len > b->size) resize_buffer(b, b->size * 2);
  memcpy(b->data + b->len, str, len * sizeof(unsigned char));
  b->len += len;
}

/*
* Write the buffer out to the given file.
*/
void write_buffer(buffer* b, char const* filename) {
  FILE* fp = fopen(filename, "wb");
  if (!fp) error("fopen()");
  fwrite(b->data, sizeof(unsigned char), b->len, fp);
  fclose(fp);
}
