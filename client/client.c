#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"

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

buffer* compile_file(int sock, struct addrinfo* serv_addr) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    fprintf(stdout, "waiting for data... ");
    buffer* buf = create_buffer(BUFFER_LEN);
    buffer* full_payload = create_buffer(BUFFER_LEN);
    header h;
    ssize_t total_bytes_recv = 0;
    int chunks = 0;
    do {
        buf->len = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
        if (buf->len < 0) error("recvfrom()");
        total_bytes_recv += buf->len;
        h = extract_header(buf);
        full_payload->len = h.data[UP_TOTAL_SIZE];
        if (full_payload->size < h.data[UP_TOTAL_SIZE])
            resize_buffer(full_payload, h.data[UP_TOTAL_SIZE]);
        assemble_datagram(full_payload, buf);
        ++chunks;
        fprintf(stderr, "%d byte datagram received\n", buf->len);
    } while (total_bytes_recv == 0 || total_bytes_recv < full_payload->len);
    fprintf(stdout, "%d bytes of data received in %d chunks\n", full_payload->len, chunks);
    delete_buffer(buf);
    return full_payload;
}

void send_request(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t request, uint32_t data) {
    buffer* message = create_message(0, password, request, 0, 0, 0, 0);
    fprintf(stdout, "sending request [%d:%d]\n", request, data);
    ssize_t bytes_sent = sendto(sock, message->data, message->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
    if (bytes_sent != UP_HEADER_LEN) error("sendto()");
}

buffer* receive_request(int sock, struct addrinfo* serv_addr) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    buffer* buf = create_buffer(BUFFER_LEN);
    buf->len = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
    if (buf->len < 0) error("recvfrom()");
    return buf;
}

int main(int argc, char** argv) {
    int port, sock;
    char hostname[BUFFER_LEN];
    struct addrinfo* serv_addr;
    uint32_t password;
    hostname[0] = '\0';

    if (argc < 3) {
        fprintf(stderr, "usage: %s udp_hostname udp_port\n", argv[0]);
        return -1;
    }

    // read in the required arguments
    strncpy(hostname, argv[1], BUFFER_LEN - 1);
    //resolve port
    port = atoi(argv[2]);
    if (port == 0) error("port number");
    // connect to the UDP server
    sock = connect_udp(hostname, argv[2], &serv_addr);

    for (;;) {
        send_request(sock, serv_addr, 0, CONNECT, 0);
        buffer* buf = receive_request(sock, serv_addr);
        header h = extract_header(buf);
        password = h.data[UP_IDENTIFIER];
        delete_buffer(buf);
        send_request(sock, serv_addr, password, CONNECT, 0);
        /* TODO: if fail somehow, call continue */
        break;
    }
    send_request(sock, serv_addr, password, GPS, 0);
    buffer* buf = receive_request(sock, serv_addr);
    header h = extract_header(buf);
    if (h.data[UP_CLIENT_REQUEST] != GPS) error("invalid acknowledgement");
    delete_buffer(buf);
    buffer* full_payload = compile_file(sock, serv_addr);
    /* just print to stdout for the time being */
    fprintf(stdout, "===== BEGIN DATA =====\n");
    fwrite(full_payload->data, full_payload->len, 1, stdout);
    fprintf(stdout, "\n====== END DATA ======\n");
    delete_buffer(full_payload);
    send_request(sock, serv_addr, password, QUIT, 0);

    close(sock);
    freeaddrinfo(serv_addr);
    return 0;
}
