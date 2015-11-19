#include "../utility.h"
#include "../udp_protocol.h"

#define ROBOT_PORT 8081
#define BUFFER_LEN 512

// connects to the specified host and returns the socket identifier
int tcp_connect(char const* server_name, int port) {
  int sock;
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    error("socket()");
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(server_name);
  serv_addr.sin_port = htons(port);
  if (serv_addr.sin_addr.s_addr == -1) {
    struct hostent* host = gethostbyname(server_name);
    serv_addr.sin_addr.s_addr = *((unsigned long*)host->h_addr_list[0]);
  }
  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    error("connect()");
  return sock;
}

// binds to the specified port and returns the socket identifier
int udp_server(char const* port_name) {
  int sock;
  struct addrinfo addr_criteria;
  memset(&addr_criteria, 0, sizeof(addr_criteria));
  addr_criteria.ai_family = AF_UNSPEC;
  addr_criteria.ai_flags = AI_PASSIVE;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;
  struct addrinfo* serv_addr;
  if (getaddrinfo(NULL, port_name, &addr_criteria, &serv_addr))
    error("getaddrinfo()");
  if ((sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol)) < 0)
    error("socket()");
  if (bind(sock, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0)
    error("bind()");
  freeaddrinfo(serv_addr);
  return sock;
}

int main(int argc, char** argv) {
  int i, id, port;
  int http_sock, udp_sock;
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
  // set up the sockets
  http_sock = tcp_connect(hostname, port);
  udp_sock = udp_server(argv[3]);
  // execution loop
  unsigned char recv_buf[BUFFER_LEN];
  for (;;) {
    
  }
  return 0;
}
