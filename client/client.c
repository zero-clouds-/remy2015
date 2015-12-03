#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"

#define BUFFER_LEN 512
#define TIMEOUT_SEC 1

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

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    int chunks = 0;
    do {
        buf->len = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
        if (buf->len < 0) error("recvfrom()");

        h = extract_header(buf);
        full_payload->len = h.data[UP_TOTAL_SIZE];
    
        if (full_payload->size < h.data[UP_TOTAL_SIZE])
            resize_buffer(full_payload, h.data[UP_TOTAL_SIZE]);
    
        assemble_datagram(full_payload, buf); //in udp_protocol.c
        ++chunks;
        fprintf(stderr, "%d byte datagram received\n", buf->len);
        total_bytes_recv += h.data[UP_PAYLOAD_SIZE];
    } while (total_bytes_recv == 0 || total_bytes_recv < full_payload->len);
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    fprintf(stdout, "%d bytes of data received in %d chunks\n", (int)total_bytes_recv, chunks);
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
    buffer* message = create_message(0, password, request, data, 0, 0, 0);
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
void move_robot(int N, int L, int sock, struct addrinfo* serv_addr, uint32_t password);

/* int main
* Main function of the client to communicate with proxy.
*/
int main(int argc, char** argv) {
    int i, flags;                     // variables for argument parsing
    int sock,                         // udp socket to proxy
        sides,                        // number of sides of first shape
        lengths;                      // length of sides of shapes
    char port[BUFFER_LEN];            // port number to connect to proxy 
    char hostname[BUFFER_LEN];        // hostname of server
    struct addrinfo* serv_addr;
    uint32_t password;                // password required provided by server
    char usage[BUFFER_LEN];           // how to use this program

    // setup basics
    hostname[0] = '\0';
    snprintf(usage, BUFFER_LEN, "usage: %s -h <udp_hostname> -p <udp_port> -n <number_of_sides> -l <length_of_sides>\n", argv[0]);

    // read in arguments
    if (argc < 9) error(usage);

    // read in the required arguments
    flags = 0;
    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') continue;
        if (argv[i][1] == 'h') {
            strncpy(hostname, argv[++i], BUFFER_LEN - 1);
            flags |= (1 << 0);
        } else if (argv[i][1] == 'p') {
            strncpy(port, argv[++i], BUFFER_LEN - 1);
            flags |= (1 << 1);
        } else if (argv[i][1] == 'n') {
            sides = atoi(argv[++i]);
            flags |= (1 << 2);
        } else if (argv[i][1] == 'l') {
            lengths = atoi(argv[++i]);
            flags |= (1 << 3);
        } else if (argv[i][1] == 'i') { // interactive mode flag
            flags |= (1 << 4);
        } else error(usage);
    }
    
    if ((flags & 0xF) != 0xF) error(usage);

    //resolve port
    //port = atoi(argv[4]);
    if (atoi(port) == 0) error("port number");
    
    //check N
    if (sides < 4 || sides > 8)
        error("parameter error with sides: 4 <= N <= 8");
    if (lengths <= 0) error("lengths");
    
    // connect to the UDP server
    sock = connect_udp(hostname, port, &serv_addr);

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

    if ((flags & 0x10)) {
        fputs("interactive mode (type help for list of commands)\n", stdout);
        for (;;) {
            char input_buffer[BUFFER_LEN];
            fprintf(stdout, "%s> ", hostname);
            fgets(input_buffer, BUFFER_LEN - 1, stdin);
            char* c = strtok(input_buffer, " \n");
            fprintf(stdout, "got [%s]\n", c);
            if (!strcmp(c, "image")) {
              get_thing(sock, serv_addr, password, IMAGE);
            } else if (!strcmp(c, "gps")) {
              get_thing(sock, serv_addr, password, GPS);
            } else if (!strcmp(c, "dgps")) {
              get_thing(sock, serv_addr, password, dGPS);
            } else if (!strcmp(c, "lasers")) {
              get_thing(sock, serv_addr, password, LASERS);
            } else if (!strcmp(c, "move")) {
                c = strtok(NULL, " \n");
                send_request(sock, serv_addr, password, MOVE, atoi(c));
            } else if (!strcmp(c, "turn")) {
                c = strtok(NULL, " \n");
                send_request(sock, serv_addr, password, MOVE, atoi(c));
            } else if (!strcmp(c, "stop")) {
              send_request(sock, serv_addr, password, STOP, 0);
            } else if (!strcmp(c, "quit")) {
              send_request(sock, serv_addr, password, QUIT, 0);
            } else if (!strcmp(c, "help")) {
                fputs("commands:\n"
                      "  image\n"
                      "  gps\n"
                      "  dgps\n"
                      "  lasers\n"
                      "  move\n"
                      "  turn\n"
                      "  stop\n"
                      "  quit\n"
                      "  help\n", stdout);
            }
        }
    } else {
    }

    //example of move_robot below for N and N-1 - check out the first example, it works
    //to watch the robot move: http://169.55.155.236:8081/stream?topic=/robot_11/image?width=600?height=500
    move_robot(sides, lengths, sock, serv_addr, password);
    //move_robot(sides - 1, lengths, sock, serv_addr, password);

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
    uint32_t data = 0;

    if(command == MOVE || command == TURN){
        data = 1;
        printf("move or turn command sent\n");
    }

    send_request(sock, serv_addr, password, command, data);
    buffer* buf = receive_request(sock, serv_addr);
    header h = extract_header(buf);
    
    if (h.data[UP_CLIENT_REQUEST] != command) error("invalid acknowledgement");
    
    delete_buffer(buf);
    buffer* full_payload = compile_file(sock, serv_addr);
    
    /* just print to timestamped file */
    fprintf(stdout, "===== BEGIN DATA =====\n");
    fwrite(full_payload->data, full_payload->len, 1, stdout);
    fprintf(stdout, "\n====== END DATA ======\n");
    
    delete_buffer(full_payload);
}

/* write_data_to_file
 *   int sock - socket to communicate over
 *   struct addrinfo* serv_addr
 *   uint32_t password - used by proxy to identify this client
 *   uint32_t command - request being made to the proxy
 *
 * writes data to a given file rather than standard out, otherwise the same as
 * getthing()  
 */
void write_data_to_file(int vertex, int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command) {
    
    uint32_t data = 0;
    send_request(sock, serv_addr, password, command, data);
    buffer* buf = receive_request(sock, serv_addr);
    header h = extract_header(buf);
    
    if (h.data[UP_CLIENT_REQUEST] != command) error("invalid acknowledgement");
    
    delete_buffer(buf);
    buffer* full_payload = compile_file(sock, serv_addr);
    
    /* print to timestamped file */
    char * sensor_type = (char *)(malloc(16)); //max size
    char * time_stamp = (char *)malloc(25); //format: Sun Aug 19 02:56:02 2012 = 24 chars 
    char * filename;

    switch(command) {
        case IMAGE:
            sprintf(sensor_type, "Vertex%d-IMAGE-", vertex);
            break;
        case STOP: 
            //not technically valid, but it returns GPS anyway so 
            //saves us from sending a redundant GPS request
            sprintf(sensor_type, "Vertex%d-GPS-", vertex);    
            break;
        case GPS:
            sprintf(sensor_type, "Vertex%d-GPS-", vertex);  
            break;
        case dGPS:
            sprintf(sensor_type, "Vertex%d-dGPS-", vertex);  
            break;
        case LASERS:
            sprintf(sensor_type, "Vertex%d-LASERS-", vertex);  
            break;
        default: error("invalid sensor");
    }   

    time_t rawtime;
    struct tm * timeinfo;
    
    time(&rawtime);
    timeinfo = localtime (&rawtime);                                                  
    strftime(time_stamp, 30, "%c",timeinfo);
    
    filename = (char *)malloc(strlen(sensor_type) + strlen(time_stamp) + 1);
    strcpy(filename, sensor_type);
    strcat(filename, time_stamp);
 
    write_buffer(full_payload, filename);
    delete_buffer(full_payload);
    printf("%s written out...\n", sensor_type);
}


/* void write_out_sensor_data 
* write out all sensors to a timestamped file.
*/
void write_out_sensor_data(int vertex, int sock, struct addrinfo* serv_addr, uint32_t password) {
        //first get robot to stop, and this gives us GPS data anyway
        write_data_to_file(vertex, sock, serv_addr, password, STOP);
        //other sensors to be added as we want them...
}


/* void move_robot
* Moves robot of a shape N by continually sending move, turn, and stop requests
* to the server on one socket, over a UDP connection. Receives dGPS, GPS information
* at each vertex.
*/ 
void move_robot(int N, int L, int sock, struct addrinfo* serv_addr, uint32_t password) {
    //calculate sleep times, calculate by 1000000 to accomodate for usleep conversion 
    unsigned turnSleepTime = (int)((14.0) / (N)); 
    unsigned moveSleepTime = L;

    int i;
    for(i = 0; i < N; i++) {
        //move robot using get_thing
        printf("* robot moving...\n");
        get_thing(sock, serv_addr, password, MOVE);
        if(usleep((moveSleepTime * 1000000)) < 0) 
            error("usleep(moveSleepTime)");
 
        //stop robot using get_thing
        printf("* robot stoping...\n");
        get_thing(sock, serv_addr, password, STOP);

        //turn robot using get_thing
        printf("* robot turning for %d seconds...\n", turnSleepTime); 
        get_thing(sock, serv_addr, password, TURN);
        if(usleep((turnSleepTime * 1000000)) < 0) 
            error("usleep(turnSleepTime)");

        //stop robot again, write data to sensors
        printf("* robot stopping, has moved %d side(s)...\n", (i + 1));
        write_out_sensor_data((i + 1), sock, serv_addr, password);

    } //repeat until shape is drawn 
    printf("* robot drew shape\n");
}
