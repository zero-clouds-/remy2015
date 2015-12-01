#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"
#include <time.h>
#include <unistd.h>

// robot ports
#define IMAGE_PORT  8081
#define GPS_PORT    8082
#define LASERS_PORT 8083
#define dGPS_PORT   8084
#define BUFFER_LEN  512

typedef struct robot_info {
    int id;
    int http_sock[4]; // 0 = image, 1 = GPS/move/turn/stop, 2 = Lasers, 3 = dGPS
    char* name;
} robot_stat;

typedef struct serverstatus_info {
    struct sockaddr_in cliaddr;
    socklen_t size;
    uint32_t password;
    int connected; // 0 not connected, 1 waiting for response, 2 connected
    int udp_sock;
    robot_stat r_stat;
} server_stat;


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

int timeout_setup(int socket, struct timeval timeout);
void protocol1(buffer* recv_buffer, server_stat* status);
void protocol2(buffer* recv_buffer, server_stat* status);
void print_header(buffer* buf);
int check_version(const header* msg_header);
int check_pass(const header* msg_header, uint32_t pass);
uint32_t get_command(const header* msg_header);
ssize_t udp_send(buffer* recv_buffer, server_stat* status);
void quit(buffer* recv_buf, server_stat* status);
int request_command(buffer* recv_buf, server_stat* status);
int http_get_content_length(char* http_msg);
unsigned char* http_get_data(char* http_msg, int length);

int main(int argc, char** argv) {
    struct timeval t_out;
    int i, iflag, hflag, nflag, pflag;
    server_stat status;
    char *port, hostname[BUFFER_LEN];
    hostname[0] = '\0';
    if (argc != 9) {
        fprintf(stderr, "usage: %s -i <robot_id> -n <robot-name> -h <http_hostname> -p <udp_port>\n", argv[0]);
        return -1;
    }

    // read in the required arguments
    iflag = hflag = pflag = 0;
    for (i = 1; i < argc; i+=2) {
        if (strcmp(argv[i], "-i") == 0) {
            status.r_stat.id = atoi(argv[i + 1]);
            iflag = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            strncpy(hostname, argv[i + 1], BUFFER_LEN - 1);
            hflag = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            port = argv[i + 1];
            pflag = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            status.r_stat.name = argv[i + 1];
            nflag = 1;
        }else {
            fprintf(stderr, "usage: %s -i <robot_id> -h <http_hostname> -n <robot_name> -p <udp_port>\n", argv[0]);
            return -1;
        }
    }
    if (!(iflag && hflag && nflag && pflag)) {
        fprintf(stderr, "usage: %s -i <robot_id> -h <http_hostname> -n <robot_name> -p <udp_port>\n", argv[0]);
        return -1;
    }

    // set up the sockets
    status.r_stat.http_sock[0] = tcp_connect(hostname, IMAGE_PORT);
    status.r_stat.http_sock[1] = tcp_connect(hostname, GPS_PORT);
    status.r_stat.http_sock[2] = tcp_connect(hostname, LASERS_PORT);
    status.r_stat.http_sock[3] = tcp_connect(hostname, dGPS_PORT);
    status.udp_sock = udp_server(port);

    // timeouts
    t_out.tv_sec = 0;
    t_out.tv_usec = 0;
    timeout_setup(status.udp_sock, t_out);

    // execution loop
    srand(time(NULL));
    status.size = sizeof(status.cliaddr);
    status.password = rand() + 1;
    status.connected = 0;
    buffer* recv_buf = create_buffer(BUFFER_LEN);
    for (;;) {

        unsigned char *temp = calloc(BUFFER_LEN, 1);
        int f = 10000;
        if ((f = recvfrom(status.udp_sock, temp, BUFFER_LEN, 0,
                    (struct sockaddr *)(&status.cliaddr), &status.size)) <= 0) {
            fprintf(stderr, "recvfrom()\n");
        } else {

            // decipher message
            clear_buffer(recv_buf);
            append_buffer(recv_buf, temp, f);
            header msg_header = extract_header(recv_buf);
            printf("data size received: %d\n", f);
            printf("Data: %d\n", ((uint32_t*)(temp))[2]);
            printf("Data: %d\n", ((uint32_t*)(recv_buf->data))[2]);
            printf("Data: %d\n", msg_header.data[2]);

            // send to correct protocol
            void (*protocol_func)(buffer*, server_stat*);
            protocol_func = check_version(&msg_header) ? &protocol1:&protocol2;
            protocol_func(recv_buf, &status);
        }
    }
    return 0;
}

int timeout_setup(int socket, struct timeval timeout) {
    if (setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0) {
        error("setsockopt failed\n");
        return 0;
    }
    if (setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
               sizeof(timeout)) < 0) {
        error("setsockopt failed\n");
        return 0;
    }
    return 1;
}

/* void protocol1
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 * Processes the recv_buf as the class protocol dictates
 */
void protocol1(buffer* recv_buf, server_stat* status){
    struct timeval timeout;
    struct timeval timeout_0;
    // timeouts
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    timeout_0.tv_sec = 0;
    timeout_0.tv_usec = 0;

    timeout_setup(status->r_stat.http_sock[0], timeout);
    timeout_setup(status->r_stat.http_sock[1], timeout);
    timeout_setup(status->r_stat.http_sock[2], timeout);
    timeout_setup(status->r_stat.http_sock[3], timeout);
    timeout_setup(status->udp_sock, timeout);

    // giant mess of conditionals
    fprintf(stdout, "version 1 protocol\n");
    int error = 0;
    header msg_header = extract_header(recv_buf);
    if (status->connected == 0) {
        if (check_pass(&msg_header, 0)) {
            if (get_command(&msg_header) == CONNECT) {
                printf("reply with password\n");
                status->connected = 1;
                msg_header.data[1] = status->password;
                insert_header(recv_buf, msg_header);
                if (udp_send(recv_buf, status) < 0) {
                    fprintf(stderr, "sendto()\n");
                }
            } else {
                printf("ignore1\n");
            }
        } else {
            printf("ignore2\n");
        }
    } else if (status->connected == 1) {
        if (check_pass(&msg_header, status->password)) {
            if (get_command(&msg_header) == CONNECT) {
                status->connected = 2;
                timeout_setup(status->udp_sock, timeout_0);
                printf("Connected to a client\n");
            } else {
                printf("ignore3\n");
            }
        } else {
            printf("ignore4\n");
        }
    } else {
        if (!check_pass(&msg_header, status->password)) {
            printf("ignore5\n");
        } else {
            switch (get_command(&msg_header)) {
                case CONNECT:
                    printf("ignore6\n");
                    break;
                case QUIT:
                    quit(recv_buf, status);
                    break;
                case IMAGE || GPS || dGPS || LASERS || MOVE || TURN || STOP:
                    printf("processing command\n");
                    error = request_command(recv_buf, status);
                    break;
                case GPS:
                    printf("HURRAH\n");
                    error = request_command(recv_buf, status);
                    break;
                default:
                    fprintf(stderr, "error with client request\n");
                    error = -2;
                    break;
            }
        }
    }
    if (error == -2) {
        fprintf(stderr, "invalid client request\n");
    } else if (error == -1) {
        fprintf(stderr, "error in http response\n");
    }
}

/* void protocol2
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 */
void protocol2(buffer* recv_buffer, server_stat* status) {
    fprintf(stdout, "version 2 protocol\n");
}

void print_header(buffer* buf) {
    header msg_header = extract_header(buf);
    printf("Version:  %d\n", msg_header.data[UP_VERSION]);
    printf("Password: %d\n", msg_header.data[UP_IDENTIFIER]);
    printf("Request:  %d\n", msg_header.data[UP_CLIENT_REQUEST]);
}

/* int check_version
 *   const header* msg_header - protocol header
 * Returns the version identifier of the client message
 */
int check_version(const header* msg_header) {
    return msg_header->data[UP_VERSION] == 0;
}

/* int check_pass
 *   const header* msg_header - protocol header
 * Returns 1 if the password in the header matches the server password
 * Returns 0 otherwise
 */
int check_pass(const header* msg_header, uint32_t pass) {
    return msg_header->data[UP_IDENTIFIER] == pass;
}

/* uint32_t get_command
 *   const header* msg_header - protocol header
 * Returns the command requested by the client
 */
uint32_t get_command(const header* msg_header) {
    return msg_header->data[UP_CLIENT_REQUEST];
}

/* ssize_t udp_send
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 * Echo's the client's message back to acknowledge its receipt
 * Returns the number of bytes sent
 */
ssize_t udp_send(buffer* recv_buf, server_stat* status) {
    return sendto(status->udp_sock, recv_buf->data, recv_buf->len, 0,
                            (struct sockaddr *)(&status->cliaddr), status->size);
}

/* void quit
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 * Acknowledges message
 * Resets password
 * Resets connection status
 */
void quit(buffer* recv_buf, server_stat* status) {
    printf("breaking connection\n");
    udp_send(recv_buf, status);
    status->password = rand();
    status->connected = 0;
}

/* void request_image
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 * Sends an acknowledgement to the client
 * Requests "stuff" from the robot
 * Forwards data from robot to client
 * Returns -2 if error processing client request
 *         -1 if error in http response
 *          1 if successful
 */
int request_command(buffer* recv_buf, server_stat* status) {
    char http_message[1000]; // used to hold http message from robot
    unsigned char* data;     // points to the data section of http_message
    unsigned char* itr;      // iterator pointing to beginning of next data section to be sent to client
    int n, content_len;      // length of the http data section
    header request_header;   // header from the client
    header response_header;  // header for messages to the client
    buffer* response;        // buffer to send to client
    int socket;              // index for which status.r_stat.http_sock[] to use

    // acknowledgement of command to client
    udp_send(recv_buf, status);
    request_header = extract_header(recv_buf);

    // create the http request
    switch (request_header.data[UP_CLIENT_REQUEST]) {
        case IMAGE:
            snprintf(http_message, 100, "GET /snapshot?topic=/robot_%d/image?width=600?height=500 HTTP/1.1\r\n\r\n", status->r_stat.id);
            socket = 0;
            break;
        case GPS:
            printf("contacting gps port\n");
            snprintf(http_message, 100, "GET /state?id=%s HTTP/1.1\r\n\r\n", status->r_stat.name);
            socket = 1;
            break;
        case LASERS:
            snprintf(http_message, 100, "GET /state?id=%s HTTP/1.1\r\n\r\n", status->r_stat.name);
            socket = 2;
            break;
        case dGPS:
            snprintf(http_message, 100, "GET /state?id=%s HTTP/1.1\r\n\r\n", status->r_stat.name);
            socket = 3;
            break;
        case MOVE:
            snprintf(http_message, 100, "GET /twist?id=%s&lx=%d HTTP/1.1\r\n\r\n", status->r_stat.name, request_header.data[UP_REQUEST_DATA]);
            socket = 1;
            break;
        case TURN:
            snprintf(http_message, 100, "GET /twist?id=%s&az=%d HTTP/1.1\r\n\r\n", status->r_stat.name, request_header.data[UP_REQUEST_DATA]);
            socket = 1;
            break;
        case STOP:
            snprintf(http_message, 100, "GET /twist?id=%s&lx=0 HTTP/1.1\r\n\r\n", status->r_stat.name);
            socket = 1;
            break;
        default:
            fprintf(stderr, "invalid client request\n");
            return -2;
            break;
    }

    
    // send the http request
    write(status->r_stat.http_sock[socket], http_message, strlen(http_message));

    // receive a response from the robot
    n = read(status->r_stat.http_sock[socket], http_message, 1000);
    if (n < 0) {
        fprintf(stderr, "read()\n");
    }
    char *useless1, *useless2;
    useless1 = strdup(http_message);
    useless2 = strdup(http_message);
    // decipher http message
    char *t = strtok(http_message, " ");
    t = strtok(NULL, " ");
    
    // check for '200 OK'
    if (atoi(t) != 200) {
        fprintf(stderr, "error with http\n");
    }
    
    // get length of data section of http message
    if ((content_len = http_get_content_length(useless1)) != 0) {
        if ((data = http_get_data(useless2, content_len)) == NULL) {
            fprintf(stderr, "http_get_data()\n");
        }
    }

    // build the header
    response_header.data[UP_VERSION] = 0;
    response_header.data[UP_IDENTIFIER] = ((uint32_t*)(recv_buf->data))[1];
    response_header.data[UP_CLIENT_REQUEST] = ((uint32_t*)(recv_buf->data))[2];
    response_header.data[UP_REQUEST_DATA] = ((uint32_t*)(recv_buf->data))[3];
    response_header.data[UP_BYTE_OFFSET] = 0;
    response_header.data[UP_TOTAL_SIZE] = content_len;

    // start sending data
    response = create_buffer(BUFFER_LEN);
    itr = (unsigned char*) data;
    while (content_len > 0) {
        // adjust the header
        response_header.data[UP_BYTE_OFFSET] = (itr - data);
        response_header.data[UP_PAYLOAD_SIZE] = (content_len - UP_MAX_PAYLOAD) > 0 ? UP_MAX_PAYLOAD:content_len;

        // pack the header
        insert_header(response, response_header);
        fprintf(stderr, "appending %d bytes to %d bytes in %d-sized buffer\n", response_header.data[UP_PAYLOAD_SIZE], response->len, response->size);
        // pack the data
        append_buffer(response, itr, response_header.data[UP_PAYLOAD_SIZE]);
        
        // adjust pointer and amount of data left to send
        itr += response_header.data[UP_PAYLOAD_SIZE];
        content_len -= response_header.data[UP_PAYLOAD_SIZE];

        printf("version: %d\n", ((uint32_t*)(response->data))[0]);
        printf("pass: %d\n", ((uint32_t*)(recv_buf->data))[1]);
        printf("request: %d\n", ((uint32_t*)(recv_buf->data))[2]);
        // send to client
        int num = udp_send(response, status);
        printf("size sent: %d\n", num);
        // clean up
        clear_buffer(response);
    }
    return 1;
}

/* int http_get_content_length
 *   char* http_msg - array containing an http response from the robot
 * Returns the Content-Length field as an integer
 */
int http_get_content_length(char* http_msg) {
    char* t;
    if ((t = strstr(http_msg, "Content-Length:")) != NULL) {
        strtok(t, " ");
        return atoi(strtok(NULL, "\r\n"));
    }
    printf("error getting content length\n");
    return 0;
}

/* unsigned char* http_get_data
 *   char* http_msg - array containing an http response from the robot
 * Returns a pointer to the beginning of the data section of the http_message
 */
unsigned char* http_get_data(char* http_msg, int length) {
    char *t, temp[length + 1];
    if ((t = strstr(http_msg, "\r\n\r\n")) != NULL) {
        t += strlen("\r\n\r\n");
        printf("%s\n", t);
        snprintf(temp, length, "%s", t);
        return (unsigned char*)t;
    }
    return NULL;
}
