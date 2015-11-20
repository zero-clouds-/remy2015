#ifndef UDP_PROTOCOL_H
#define UDP_PROTOCOL_H

#include "utility.h"

#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define UP_HEADER_LEN 24
#define UP_IDENTIFIER 0
#define UP_CLIENT_REQUEST 1
#define UP_REQUEST_DATA 2
#define UP_BYTE_OFFSET 3
#define UP_TOTAL_SIZE 4
#define UP_PAYLOAD_SIZE 5

typedef struct header_t {
  uint32_t data[5];
} header; 

void insert_header(buffer* datagram, header h);

header extract_header(buffer* datagram);

void separate_datagram(buffer* dst, buffer* src, int offset, int len);

void assemble_datagram(buffer* dst, buffer* src);

#endif
