#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#define ROBOT_PORT 8081
#define BUFFER_LEN 256

int main(int argc, char** argv) {
  int i, id, port;
  char hostname[BUFFER_LEN];
  hostname[0] = '\0';
  if (argc < 4) {
    fprintf(stderr, "usage: %s robot_id http_hostname udp_port\n", argv[0]);
    return -1;
  }
  id = atoi(argv[1]);
  strncpy(hostname, argv[2], BUFFER_LEN - 1);
  port = atoi(argv[3]);
  
  return 0;
}