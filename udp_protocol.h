#ifndef UDP_PROTOCOL_H
#define UDP_PROTOCOL_H

#include "utility.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define UP_HEADER_LEN 12

typedef struct header_t {
  int offset;
  int total_size;
  int payload_len;
} header; 

void insert_header(buffer* datagram, header h) {
  memcpy(datagram->data, &h.offset, sizeof(int));
  memcpy(datagram->data + 4, &h.total_size, sizeof(int));
  memcpy(datagram->data + 8, &h.payload_len, sizeof(int));
}
header extract_header(buffer* datagram) {
  header h;
  memcpy(&h.offset, datagram->data, sizeof(int));
  memcpy(&h.total_size, datagram->data + 4, sizeof(int));
  memcpy(&h.payload_len, datagram->data + 8, sizeof(int));
  return h;
}

// note that this overwrites the contents of dst; dst must be at least 316 bytes long
void separate_datagram(buffer* dst, buffer* src, int offset, int len) {
  header h;
  clear_buffer(dst);
  dst->len = (UP_HEADER_LEN + len) * sizeof(unsigned int);
  memcpy(dst->data + UP_HEADER_LEN, src->data + offset, len);
  h.offset = offset;
  h.total_size = src->len;
  h.payload_len = len;
  insert_header(dst, h);
}

// note tha src is the incoming datagram in this case
void assemble_datagram(buffer* dst, buffer* src) {
  header h = extract_header(src);
  memcpy(dst->data + h.offset, src->data + UP_HEADER_LEN, h.payload_len);
}

#endif
