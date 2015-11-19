#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#define BUFFER_LEN 256

int main(int argc, char** argv) {
  int i, port;
  char hostname[BUFFER_LEN];
  hostname[0] = '\0';
  if (argc < 3) {
    fprintf(stderr, "usage: %s udp_hostname udp_port\n", argv[0]);
    return -1;
  }
  strncpy(hostname, argv[1], BUFFER_LEN - 1);
  port = atoi(argv[2]);
  
  return 0;
}