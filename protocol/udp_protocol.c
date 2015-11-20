#include "udp_protocol.h"

void insert_header(buffer* datagram, header h) {
  memcpy(datagram->data, h.data, UP_HEADER_LEN);
}
header extract_header(buffer* datagram) {
  header h;
  memcpy(h.data, datagram->data, UP_HEADER_LEN);
  return h;
}

// note that this overwrites the contents of dst; dst must be at least 316 bytes long
void separate_datagram(buffer* dst, buffer* src, int offset, int len) {
  header h;
  clear_buffer(dst);
  dst->len = (UP_HEADER_LEN + len) * sizeof(unsigned int);
  memcpy(dst->data + UP_HEADER_LEN, src->data + offset, len);
  h.data[UP_BYTE_OFFSET] = offset;
  h.data[UP_TOTAL_SIZE] = src->len;
  h.data[UP_PAYLOAD_SIZE] = len;
  insert_header(dst, h);
}

// note tha src is the incoming datagram in this case
void assemble_datagram(buffer* dst, buffer* src) {
  header h = extract_header(src);
  memcpy(dst->data + h.data[UP_BYTE_OFFSET], src->data + UP_HEADER_LEN, h.data[UP_PAYLOAD_SIZE]);
}
