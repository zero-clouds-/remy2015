#ifndef CST_PROTOCOL_H
#define CST_PROTOCOL_H

#include "../protocol/utility.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

//deminsions of custom protocol
#define CST_MAX_PAYLOAD 370
#define CST_VERSION 0
#define CST_COMMAND 1
#define CST_SEQUENCE 2
#define CST_TOTAL_SIZE 3
#define CST_PAYLOAD_SIZE 4
#define CST_HEADER_LEN 20

// valid command encodings
#define CONNECT      0
#define IMAGE        2
#define GPS          4
#define dGPS         8
#define LASERS       16
#define MOVE         32
#define STOP_MOVE    50
#define TURN         64
#define STOP_TURN    128
#define QUIT         255
#define CLIENT_ERROR 256
#define HTTP_ERROR   512
#define DATA         1337

typedef struct cst_header_t {
  uint32_t data[5];
} cst_header; 

void insert_cvustom_header(buffer* datagram, cst_header h);

cst_header extract_custom_header(buffer* datagram);

void assemble_custom_datagram(buffer* dst, buffer* src);

buffer* create_cutom_message(uint32_t version, uint32_t command, uint32_t sequence, uint32_t total_size, uint32_t payload);

#endif
