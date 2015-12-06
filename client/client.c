#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"
#include "../protocol/custom_protocol.h"

#define BUFFER_LEN 512
#define TIMEOUT_SEC 1

typedef struct pt_t {
    double x, y;
} pt;

int pt_count = 0;
pt points[64];

struct timespec prev_time;
struct timespec curr_time;

void check_time() {
    prev_time = curr_time;
    clock_gettime(CLOCK_REALTIME, &curr_time);
}
long get_elapsed_us() {
    long ds = curr_time.tv_sec - prev_time.tv_sec;
    long dns = curr_time.tv_nsec - prev_time.tv_nsec;
    return ds * 1000000 + dns / 1000;
}

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
buffer* compile_file(int sock, struct addrinfo* serv_addr, int version) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    
    fprintf(stdout, "waiting for data... ");
    buffer* buf = create_buffer(BUFFER_LEN);
    buffer* full_payload = create_buffer(BUFFER_LEN);
    
    ssize_t total_bytes_recv = 0;

    struct timeval timeout;
    timeout.tv_sec = 5; // large timeout here to compensate for server having to wait for robot and then process response before sending back
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    int chunks = 0;
    do {
        buf->len = recvfrom(sock, buf->data, buf->size, 0, (struct sockaddr*)&from_addr, &from_addrlen);
        if (buf->len < 0) error("recvfrom()");

        header h;
        cst_header ch;
        if (version == 0) {
            h = extract_header(buf);
            full_payload->len = h.data[UP_TOTAL_SIZE];
            if (full_payload->size < h.data[UP_TOTAL_SIZE])
                resize_buffer(full_payload, h.data[UP_TOTAL_SIZE]);
            assemble_datagram(full_payload, buf); //in udp_protocol.c
            total_bytes_recv += h.data[UP_PAYLOAD_SIZE];
        }
        else {
            ch = extract_custom_header(buf);
            full_payload->len = ch.data[CST_TOTAL_SIZE];
            if (full_payload->size < ch.data[CST_TOTAL_SIZE])
                resize_buffer(full_payload, ch.data[CST_TOTAL_SIZE]);
            assemble_custom_datagram(full_payload, buf); //in udp_protocol.c
            total_bytes_recv += ch.data[CST_PAYLOAD_SIZE];
        }

        ++chunks;
        fprintf(stderr, "%d byte datagram received\n", buf->len);
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
void send_request(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t request, uint32_t data, uint32_t version) {
    buffer* message;
    if (version == 0) message = create_message(0, password, request, data, 0, 0, 0);
    else message = create_custom_message(GROUP_NUMBER, password, request, 0, 0, 0);
    fprintf(stdout, "sending request [%d:%d]\n", request, data);
    ssize_t bytes_sent = sendto(sock, message->data, message->len, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
    if (bytes_sent != message->len) error("sendto()");
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
void get_thing(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command, char const* filename, uint32_t version);
void move_robot(int N, int L, int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t version);

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
    uint32_t version;
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
        } else if (argv[i][1] == 'v') {
            version = atoi(argv[++i]);
            flags |= (1 << 5);
        } else error(usage);
    }
    
    if ((flags & 0xF) != 0xF) error(usage);

    //resolve port
    if (atoi(port) == 0) error("port number");
    
    //check N
    if (sides < 4 || sides > 8)
        error("parameter error with sides: 4 <= N <= 8");
    if (lengths <= 0) error("lengths");
    
    // connect to the UDP server
    sock = connect_udp(hostname, port, &serv_addr);

    for (;;) {
        send_request(sock, serv_addr, 0, CONNECT, 0, version);
        buffer* buf = receive_request(sock, serv_addr);
        header h = extract_header(buf);
        password = h.data[UP_IDENTIFIER];
        delete_buffer(buf);
        send_request(sock, serv_addr, password, CONNECT, 0, version);
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
                c = strtok(NULL, " \n");
                get_thing(sock, serv_addr, password, IMAGE, c, version);
            } else if (!strcmp(c, "gps")) {
                c = strtok(NULL, " \n");
                get_thing(sock, serv_addr, password, GPS, c, version);
            } else if (!strcmp(c, "dgps")) {
                c = strtok(NULL, " \n");
                get_thing(sock, serv_addr, password, dGPS, c, version);
            } else if (!strcmp(c, "lasers")) {
                c = strtok(NULL, " \n");
                get_thing(sock, serv_addr, password, LASERS, c, version);
            } else if (!strcmp(c, "move")) {
                c = strtok(NULL, " \n");
                send_request(sock, serv_addr, password, MOVE, atoi(c), version);
            } else if (!strcmp(c, "turn")) {
                c = strtok(NULL, " \n");
                send_request(sock, serv_addr, password, TURN, atoi(c), version);
            } else if (!strcmp(c, "stop")) {
                send_request(sock, serv_addr, password, STOP, 0, version);
            } else if (!strcmp(c, "quit")) {
                send_request(sock, serv_addr, password, QUIT, 0, version);
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

    move_robot(sides, lengths, sock, serv_addr, password, version);
    move_robot(sides - 1, lengths, sock, serv_addr, password, version);

    //done requesting, quit
    send_request(sock, serv_addr, password, QUIT, 0, version);

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
void get_thing(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command, char const* filename, uint32_t version) {
    // GSP GET
    uint32_t data = 0;

    if(command == MOVE || command == TURN){
        data = 1;
    }
    check_time();

    send_request(sock, serv_addr, password, command, data, version);
    buffer* buf = receive_request(sock, serv_addr);
    header h = extract_header(buf);
    
    if (h.data[UP_CLIENT_REQUEST] != command) error("invalid acknowledgement");

    delete_buffer(buf);
    buffer* full_payload = compile_file(sock, serv_addr, version);

    if (filename) {
        FILE* fp = fopen(filename, "wb");
        fwrite(full_payload->data, full_payload->len, 1, fp);
        fclose(fp);
    } else { 
        /* just print to timestamped file */
        fprintf(stdout, "===== BEGIN DATA =====\n");
        fwrite(full_payload->data, full_payload->len, 1, stdout);
        fprintf(stdout, "\n====== END DATA ======\n");
    }
    
    delete_buffer(full_payload);
    check_time();
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
void write_data_to_file(int vertex, int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command, uint32_t version) {
    
    uint32_t data = 0;
    send_request(sock, serv_addr, password, command, data, version);
    buffer* buf = receive_request(sock, serv_addr);
    header h = extract_header(buf);
    
    if (h.data[UP_CLIENT_REQUEST] != command) error("invalid acknowledgement");
    
    delete_buffer(buf);
    buffer* full_payload = compile_file(sock, serv_addr, version);
    
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
    
    filename = (char *)malloc(strlen(sensor_type) + strlen(time_stamp) + 5);
    strcpy(filename, sensor_type);
    strcat(filename, time_stamp);

    if (command == GPS || command == STOP) {
        char* xxx = strtok((char*)full_payload->data, ",");
        points[pt_count].x = atof(xxx);
        xxx = strtok(NULL, ",");
        points[pt_count].y = atof(xxx);
        ++pt_count;
    }
    if(command == IMAGE) strcat(filename, ".jpeg");
    else strcat(filename, ".txt");
 
    write_buffer(full_payload, filename);
    delete_buffer(full_payload);
    printf("%s written out...\n", sensor_type);
}


/* void write_out_sensor_data 
* At each vertex of the shape, the client must request and store data from the robot’s 
* sensors.This data is to be stored in separate time stamped files for each sensor.
*/
void write_out_sensor_data(int vertex, int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t version) {
        //first get robot to stop, and this gives us GPS data anyway
          write_data_to_file(vertex, sock, serv_addr, password, STOP, version);
          write_data_to_file(vertex, sock, serv_addr, password, dGPS, version);
          write_data_to_file(vertex, sock, serv_addr, password, LASERS, version);
  //        write_data_to_file(vertex, sock, serv_addr, password, IMAGE);    
}


/* void move_robot
* Moves robot of a shape N by continually sending move, turn, and stop requests
* to the server on one socket, over a UDP connection. Receives dGPS, GPS information
* at each vertex.
*/ 
void move_robot(int N, int L, int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t version) {
    /* ax * (pi/7) * seconds = (2 pi) / N / 1
     * seconds = ((2 PI) / N) * 7 / pi
     * seconds = 14 / N
    */
    double turnSleepTime = 14.0 / N; 
    int moveSleepTime = L;
    
    int i;
    write_out_sensor_data(0, sock, serv_addr, password, version);
    for(i = 0; i < N; i++) {
        //move
        printf("* robot moving...\n");
        get_thing(sock, serv_addr, password, MOVE, NULL, version);
        if(usleep((moveSleepTime * 1000000) - get_elapsed_us()) < 0) 
            error("usleep(moveSleepTime)");
 
        //stop robot using get_thing
        printf("* robot stopping...\n");
        get_thing(sock, serv_addr, password, STOP, NULL, version);

        //turn
        printf("* robot turning for %lf seconds...\n", turnSleepTime); 
        get_thing(sock, serv_addr, password, TURN, NULL, version);
        if(usleep((int)(turnSleepTime * 1000000) - get_elapsed_us()) < 0) 
            error("usleep(turnSleepTime)");

        //stop robot again, write data to sensors
        printf("* robot stopping, has moved %d side(s)...\n", (i + 1));
        write_out_sensor_data((i + 1), sock, serv_addr, password, version);

    } //repeat until shape is drawn 
    printf("* robot drew shape\n");
    
    FILE* tt = fopen("points", "w");
    for (i = 0; i < pt_count; ++i) {
        fprintf(tt, "%lf %lf\n", points[i].x, points[i].y);
    }
    fclose(tt);
    
    
}
