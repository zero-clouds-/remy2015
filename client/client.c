#include "../utility.h"
#include "../udp_protocol.h"

#define BUFFER_LEN 512

int connect_udp(char const* hostname, char const* port_name, struct addrinfo** serv_addr) {
  int sock;
  struct addrinfo addr_criteria;
  memset(&addr_criteria, 0, sizeof(addr_criteria));
  addr_criteria.ai_family = AF_UNSPEC;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;
  if (getaddrinfo(hostname, port_name, &addr_criteria, serv_addr))
    error("getaddrinfo()");
  if ((sock = socket((*serv_addr)->ai_family, (*serv_addr)->ai_socktype, (*serv_addr)->ai_protocol)) < 0)
    error("socket()");
  return sock;
}

int main(int argc, char** argv) {
  int i, sock;
  char hostname[BUFFER_LEN];
  struct addrinfo* serv_addr;
  hostname[0] = '\0';
  if (argc < 3) {
    fprintf(stderr, "usage: %s udp_hostname udp_port\n", argv[0]);
    return -1;
  }
  // read in the required arguments
  strncpy(hostname, argv[1], BUFFER_LEN - 1);
  // connect to the UDP server
  sock = connect_udp(hostname, argv[2], &serv_addr);
  for (;;) {

  }
  freeaddrinfo(serv_addr);
  return 0;
}
