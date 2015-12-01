#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"

#include <unistd.h>     /* for close() */

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

void send_request(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t request, uint32_t data, buffer* buf) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    buffer* next_message = create_message(0, password, request, 0, 0, 0, 0);
    fprintf(stdout, "intending to send request \"%d:%d\"\n", request, data);
    header x = extract_header(next_message);
    fprintf(stdout, "actually sending request \"%d:%d\"\n", x.data[UP_CLIENT_REQUEST], x.data[UP_REQUEST_DATA]);
    ssize_t bytes_sent = sendto(sock, next_message->data, next_message->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
    if (bytes_sent != UP_HEADER_LEN) error("sendto()");
    fprintf(stdout, "waiting for data... ");
    ssize_t bytes_recv = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
    if (bytes_recv < 0) error("recvfrom()");
    buf->len = bytes_recv;
    fprintf(stdout, "data received\n");
    header h = extract_header(buf);
    buf->data[h.data[UP_PAYLOAD_SIZE] + UP_HEADER_LEN] = '\0';
    fprintf(stdout, "%s\n", (char*)buf->data + h.data[UP_PAYLOAD_SIZE]);
    buffer* close_message = create_message(0, password, QUIT, 0, 0, 0, 0);
    fprintf(stdout, "sending quit request\n");
    bytes_sent = sendto(sock, close_message->data, close_message->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
    if (bytes_sent < 0) error("sendto()"); 
}

int main(int argc, char** argv) {
    int port, sock;
    char hostname[BUFFER_LEN];
    struct addrinfo* serv_addr;
    uint32_t password;
    int connected = 0;
    hostname[0] = '\0';

    if (argc < 3) {
        fprintf(stderr, "usage: %s udp_hostname udp_port\n", argv[0]);
        return -1;
    }

    // read in the required arguments
    strncpy(hostname, argv[1], BUFFER_LEN - 1);
    //resolve port
    port = atoi(argv[2]);
    // connect to the UDP server
    sock = connect_udp(hostname, argv[2], &serv_addr);

    buffer* buf = create_buffer(BUFFER_LEN);
    buffer* connect_message = create_message(0, 0, CONNECT, 0, 0, 0, 0);

    //Grace - implement client to receive server response
    for (;;) {
        if (connected == 0) {
            struct sockaddr_storage from_addr;
            socklen_t from_addrlen = sizeof(from_addr);
            fprintf(stdout, "sending connect request\n");
            ssize_t bytes_sent = sendto(sock, connect_message->data, connect_message->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
            if (bytes_sent < 0) error("sendto()");
            fprintf(stdout, "waiting for password... ");
            ssize_t bytes_recv = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
            if (bytes_recv < 0) error("recvfrom()");
            buf->len = bytes_recv;
            fprintf(stdout, "password received\n");
            header h = extract_header(buf);
            password = h.data[UP_IDENTIFIER];
            buffer* ack = create_message(0, password, CONNECT, 0, 0, 0, 0);
            fprintf(stdout, "sending acknowledgement\n");
            bytes_sent = sendto(sock, ack->data, ack->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
            if (bytes_sent < 0) error("sendto()");
            delete_buffer(ack);
            connected = 1;
        } else {
            sleep(3);
            send_request(sock, serv_addr, password, GPS, 0, buf);
            break;
        }
    }

    close(sock);
    freeaddrinfo(serv_addr);
    return 0;
}
