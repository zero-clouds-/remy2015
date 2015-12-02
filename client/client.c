#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"

#define BUFFER_LEN 512

/* int connect_udp
 *   const char* hostname - hostname to connect to
 *   const char* port_name - 
 *   struct addringo* serv_addr
 * Returns the pointer to a socket if successful
 * errors and dies otherwise
 */
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

/* buffer* compile_file
 *   int sock - socket to communicate over
 *   struct addrinfo* serv_addr - server information
 * Returns a buffer with data collected from server
 */
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
    
        assemble_datagram(full_payload, buf); //in udp_protocol.c
        ++chunks;
        fprintf(stderr, "%d byte datagram received\n", buf->len);
    
    } while (total_bytes_recv == 0 || total_bytes_recv < full_payload->len);
    
    fprintf(stdout, "%d bytes of data received in %d chunks\n", total_bytes_recv - (chunks * 28), chunks);
    delete_buffer(buf);
    
    return full_payload;
}

/* void send_request
 *   struct addrinfo* servaddr
 *   uint32_t password - password the proxy uses to identify this client
 *   uint32_t request - command being sent (GPS, dGPS, LASERS, IMAGE, MOVE, TURN, STOP)
 *   uint32_t data - any data that request requires (MOVE/TURN rate)
 * errors and dies if message could not be sent
 */
void send_request(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t request, uint32_t data) {
    buffer* message = create_message(0, password, request, 0, 0, 0, 0);
    fprintf(stdout, "sending request [%d:%d]\n", request, data);
    ssize_t bytes_sent = sendto(sock, message->data, message->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
    if (bytes_sent != UP_HEADER_LEN) error("sendto()");
}

/* buffer* receive_request
 *   int sock - socket to communicate through
 *   struct addrinfo* servaddr
 * returns buffer with data from proxy on success
 * errors and dies otherwise
 */
buffer* receive_request(int sock, struct addrinfo* serv_addr) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    buffer* buf = create_buffer(BUFFER_LEN);
    buf->len = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
    if (buf->len < 0) error("recvfrom()");
    return buf;
}

//roobot situational methods
void get_thing(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command);
void send_command(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command);


/* int main
* Main function of the client to communicate with proxy.
*/
int main(int argc, char** argv) {
   int i, hflag, pflag, nflag, lflag; // variables for argument parsing
    int port,                          // port number to connect with proxy
        sock,                          // udp socket to proxy
        sides,                         // number of sides of first shape
        lengths;                       // length of sides of shapes
    char hostname[BUFFER_LEN];         // hostname of server
    struct addrinfo* serv_addr;
    uint32_t password;                 // password required provided by server
    char usage[BUFFER_LEN];            // how to use this program


    // setup basics
    hostname[0] = '\0';
    snprintf(usage, BUFFER_LEN, "usage: %s -h <udp_hostname> -p <udp_port> -n <number_of_sides> -l <length_of_sides>\n", argv[0]);

    // read in arguments
    if (argc < 9) {
        error(usage);
    }
/*
    if (argc < 3) {
        fprintf(stderr, "usage: %s udp_hostname udp_port\n", argv[0]);
        return -1;
    }
*/

    // read in the required arguments
    hflag = pflag = nflag = lflag = 0;
    for (i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-h") == 0) {
            strncpy(hostname, argv[i + 1], BUFFER_LEN - 1);
            hflag = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[i + 1]);
            pflag = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            sides = atoi(argv[i + 1]);
            nflag = 1;
        } else if (strcmp(argv[i], "-l") == 0) {
            lengths = atoi(argv[i + 1]);
            lflag = 1;
        } else {
            error(usage);
        }
    }
    if (!(hflag && pflag && nflag && lflag)) {
        error(usage);
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
    
    //by this point the server is waiting for what to do    
    get_thing(sock, serv_addr, password, GPS);
    get_thing(sock, serv_addr, password, dGPS);
    get_thing(sock, serv_addr, password, LASERS);

    //done requesting, quit
    send_request(sock, serv_addr, password, QUIT, 0);

    close(sock);
    freeaddrinfo(serv_addr);
    return 0;
}

/* void get_thing
 *   int sock - socket to communicate over
 *   struct addrinfo* serv_addr
 *   uint32_t password - used by proxy to identify this client
 *   uint32_t command - request being made to the proxy
 */
void get_thing(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command) {
    // GSP GET
    send_request(sock, serv_addr, password, command, 0);
    buffer* buf = receive_request(sock, serv_addr);
    header h = extract_header(buf);
    
    if (h.data[UP_CLIENT_REQUEST] != command) error("invalid acknowledgement");
    
    delete_buffer(buf);
    buffer* full_payload = compile_file(sock, serv_addr);
    
    /* just print to stdout for the time being */
    fprintf(stdout, "===== BEGIN DATA =====\n");
    fwrite(full_payload->data, full_payload->len, 1, stdout);
    fprintf(stdout, "\n====== END DATA ======\n");
    
    delete_buffer(full_payload);
}
