#include "custom_protocol.h"

void insert_header(buffer* datagram, header h) {
  memcpy(datagram->data, h.data, CST_HEADER_LEN);
}
header extract_header(buffer* datagram) {
  header h;
  memcpy(h.data, datagram->data, CST_HEADER_LEN);
  return h;
}

// note that this overwrites the contents of dst; dst must be at least 316 bytes long
void separate_datagram(buffer* dst, buffer* src, int offset, int len) {
  header h;
  clear_buffer(dst);
  dst->len = (CST_HEADER_LEN + len) * sizeof(unsigned char);
  memcpy(dst->data + CST_HEADER_LEN, src->data + offset, len);
  h.data[CST_BYTE_OFFSET] = offset;
  h.data[CST_TOTAL_SIZE] = src->len;
  h.data[CST_PAYLOAD_SIZE] = len;
  insert_header(dst, h);
}

// note tha src is the incoming datagram in this case
void assemble_datagram(buffer* dst, buffer* src) {
  header h = extract_header(src);
  memcpy(dst->data + h.data[CST_BYTE_OFFSET], src->data + CST_HEADER_LEN, h.data[CST_PAYLOAD_SIZE]);
}

buffer* create_message(uint32_t version, uint32_t id, uint32_t request, uint32_t data, uint32_t offset, uint32_t total_size, uint32_t payload) {
  buffer* message = create_buffer((CST_HEADER_LEN + payload) * sizeof(unsigned char));
  uint32_t header[7];
  header[CST_VERSION] = version;
  header[CST_IDENTIFIER] = id;
  header[CST_CLIENT_REQUEST] = request;
  header[CST_REQUEST_DATA] = data;
  header[CST_BYTE_OFFSET] = offset;
  header[CST_TOTAL_SIZE] = total_size;
  header[CST_PAYLOAD_SIZE] = payload;
  memcpy(message->data, header, CST_HEADER_LEN);
  return message;
}