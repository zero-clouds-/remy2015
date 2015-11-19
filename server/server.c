#include "../utility.h"

#define ROBOT_PORT 8081
#define BUFFER_LEN 256

// connects to the specified host and returns the socket identifier
int tcp_connect() {
  int sock;
  return 0;
}

// binds to the specified port and returns the socket identifier
int udp_server() {
  int sock;
  return 0;
}

int main(int argc, char** argv) {
  int i, id, port;
  char hostname[BUFFER_LEN];
  hostname[0] = '\0';
  if (argc < 4) {
    fprintf(stderr, "usage: %s robot_id http_hostname udp_port\n", argv[0]);
    return -1;
  }
  // read in the required arguments
  id = atoi(argv[1]);
  strncpy(hostname, argv[2], BUFFER_LEN - 1);
  port = atoi(argv[3]);
  
  return 0;
}