#include "udp_protocol.h"

/* void insert_header
* Prepends the datagram's data with given header information.
*/
void insert_header(buffer* datagram, header h) {
  memcpy(datagram->data, h.data, UP_HEADER_LEN);
  datagram->len = UP_HEADER_LEN;
}

/* header extract_header
* Copies header information from the datagram and fills data of given header.
*/
header extract_header(buffer* datagram) {
  header h;
  memcpy(h.data, datagram->data, UP_HEADER_LEN);
  return h;
}

/* void seperate_datagram
* note that this overwrites the contents of dst; dst must be at least 316 bytes long
*/
void separate_datagram(buffer* dst, buffer* src, int offset, int len) {
  header h;
  clear_buffer(dst);
  dst->len = (UP_HEADER_LEN + len) * sizeof(unsigned char);
  memcpy(dst->data + UP_HEADER_LEN, src->data + offset, len);
  h.data[UP_BYTE_OFFSET] = offset;
  h.data[UP_TOTAL_SIZE] = src->len;
  h.data[UP_PAYLOAD_SIZE] = len;
  insert_header(dst, h);
}

/* void assemble_datagram
* Given  a source adn destination, copy the payload size of the source into the 
* data of the destination starting at the offset. 
* NOTE: that src is the incoming datagram in this case
*/
void assemble_datagram(buffer* dst, buffer* src) {
  header h = extract_header(src);
  memcpy(dst->data + h.data[UP_BYTE_OFFSET], src->data + UP_HEADER_LEN, h.data[UP_PAYLOAD_SIZE]);
}

/* buffer * create_message
* Create a message to send by loading a new header with parameters and returning buffer. 
*/
buffer* create_message(uint32_t version, uint32_t id, uint32_t request, uint32_t data, uint32_t offset, uint32_t total_size, uint32_t payload) {
  buffer* message = create_buffer((UP_HEADER_LEN + payload) * sizeof(unsigned char));
  uint32_t header[7];
  header[UP_VERSION] = version;
  header[UP_IDENTIFIER] = id;
  header[UP_CLIENT_REQUEST] = request;
  header[UP_REQUEST_DATA] = data;
  header[UP_BYTE_OFFSET] = offset;
  header[UP_TOTAL_SIZE] = total_size;
  header[UP_PAYLOAD_SIZE] = payload;
  memcpy(message->data, header, UP_HEADER_LEN);
  message->len = UP_HEADER_LEN;
  return message;
}
