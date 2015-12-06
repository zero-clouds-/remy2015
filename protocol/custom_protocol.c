#include "custom_protocol.h"

/* void insert_header
* Prepends the datagram's data with given header information.
*/
void insert_custom_header(buffer* datagram, cst_header h) {
  memcpy(datagram->data, h.data, CST_HEADER_LEN);
  datagram->len = CST_HEADER_LEN;
}

/* header extract_header
* Copies header information from the datagram and fills data of given header.
*/
cst_header extract_custom_header(buffer* datagram) {
  cst_header h;
  memcpy(h.data, datagram->data, CST_HEADER_LEN);
  return h;
}


/* void assemble_datagram
* Given  a source adn destination, copy the payload size of the source into the 
* data of the destination starting at the offset. 
* NOTE: that src is the incoming datagram in this case
*/

void assemble_custom_datagram(buffer* dst, buffer* src) {
  cst_header h = extract_custom_header(src);
  memcpy(dst->data + (370 * h.data[CST_SEQUENCE]), src->data + CST_HEADER_LEN, h.data[CST_PAYLOAD_SIZE]);
}

/* buffer * create_message
* Create a message to send by loading a new header with parameters and returning buffer. 
*/
buffer* create_custom_message(uint32_t version, uint32_t password, uint32_t command, uint32_t sequence, uint32_t total_size, uint32_t payload) {
  buffer* message = create_buffer((CST_HEADER_LEN + payload) * sizeof(unsigned char));
  uint32_t header[6];
  header[CST_VERSION] = version;
  header[CST_PASSWORD] = password;
  header[CST_COMMAND] = command;
  if (command == DATA) {
    memcpy(message->data, header, 3);
    message->len = 3;
    return message;
  }
  header[CST_SEQUENCE] = sequence;
  header[CST_TOTAL_SIZE] = total_size;
  header[CST_PAYLOAD_SIZE] = payload;
  memcpy(message->data, header, CST_HEADER_LEN);
  message->len = CST_HEADER_LEN;
  return message;
}
