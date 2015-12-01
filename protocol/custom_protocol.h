#ifndef CUSTOM_PROTOCOL_H
#define CUSTOM_PROTOCOL_H

#include "utility.h"

#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define CST_HEADER_LEN 28
#define CST_MAX_PAYLOAD 250
#define CST_VERSION 1
#define CST_IDENTIFIER 1
#define CST_CLIENT_REQUEST 2
#define CST_REQUEST_DATA 3
#define CST_BYTE_OFFSET 4
#define CST_TOTAL_SIZE 5
#define CST_PAYLOAD_SIZE 6

// valid command encodings
#define CONNECT      0
#define IMAGE        2
#define GPS          4
#define dGPS         8
#define LASERS       16
#define MOVE         32
#define TURN         64
#define STOP         128
#define QUIT         255
#define CLIENT_ERROR 256
#define HTTP_ERROR   512

typedef struct header_t {
  uint32_t data[7];
} header; 

void insert_header(buffer* datagram, header h);

header extract_header(buffer* datagram);

void separate_datagram(buffer* dst, buffer* src, int offset, int len);

void assemble_datagram(buffer* dst, buffer* src);

buffer* create_message(uint32_t version, uint32_t id, uint32_t request, uint32_t data, uint32_t offset, uint32_t total_size, uint32_t payload);

#endif