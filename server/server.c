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
    int http_sock;
    char hostname[BUFFER_LEN];
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


/* 
* connects to the specified host and returns the socket identifier
*/
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

/*
* binds to the specified port and returns the socket identifier
*/
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

int            timeout_setup           (int socket, struct timeval timeout);
void           protocol1               (buffer* recv_buffer, server_stat* status);
void           protocol2               (buffer* recv_buffer, server_stat* status);
void           print_header            (buffer* buf);
int            check_version           (const header* msg_header);
int            check_pass              (const header* msg_header, uint32_t pass);
uint32_t       get_command             (const header* msg_header);
ssize_t        udp_send                (buffer* recv_buffer, server_stat* status);
void           quit                    (buffer* recv_buf, server_stat* status);
int            request_command         (buffer* recv_buf, server_stat* status);
int            http_get_content_length (char* http_msg);
unsigned char* http_get_data           (unsigned char* http_msg);

/*
* Main function
*/
int main(int argc, char** argv) {
    struct timeval t_out;
    int i, iflag, hflag, nflag, pflag;
    server_stat status;
    char *port;
    char usage[100];

    snprintf(usage, 100, "Usage: %s -i <robot_id> -n <robot-name> -h <http_hostname> -p <udp_port>", argv[0]);
    status.r_stat.hostname[0] = '\0';
    if (argc != 9) {
        fprintf(stderr, "%s\n", usage);
        return -1;
    }

    // read in the required arguments
    iflag = hflag = pflag = 0;
    for (i = 1; i < argc; i+=2) {
        if (strcmp(argv[i], "-i") == 0) {
            status.r_stat.id = atoi(argv[i + 1]);
            iflag = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            strncpy(status.r_stat.hostname, argv[i + 1], BUFFER_LEN - 1);
            hflag = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            port = argv[i + 1];
            pflag = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            status.r_stat.name = argv[i + 1];
            nflag = 1;
        }else {
            fprintf(stderr, "%s\n", usage);
            return -1;
        }
    }
    if (!(iflag && hflag && nflag && pflag)) {
        fprintf(stderr, "%s\n", usage);
        return -1;
    }

    // set up the udp socket
    status.udp_sock = udp_server(port);

    // timeouts
    t_out.tv_sec = 60;
    t_out.tv_usec = 0;
    timeout_setup(status.udp_sock, t_out);

    // execution loop
    srand(time(NULL));
    status.size = sizeof(status.cliaddr);
    status.password = rand() + 1;
    status.connected = 0;
    buffer* recv_buf = create_buffer(BUFFER_LEN);
    for (;;) {

        timeout_setup(status.udp_sock, t_out);
        unsigned char temp[BUFFER_LEN];
        memset(temp, '\0', BUFFER_LEN);
        int f = 10000;
        if ((f = recvfrom(status.udp_sock, temp, BUFFER_LEN, 0,
                    (struct sockaddr *)(&status.cliaddr), &status.size)) < 0) {
            buffer* quit_buf = create_message(0, status.password, QUIT, 0, 0, 0, 0);
            quit(quit_buf, &status);
        } else {

            // decipher message
            clear_buffer(recv_buf);
            append_buffer(recv_buf, temp, f);
            header msg_header = extract_header(recv_buf);
            printf("\nMessage received\n");
            printf("\tMessage size: %d\n", f);

            // send to correct protocol
            void (*protocol_func)(buffer*, server_stat*);
            protocol_func = check_version(&msg_header) ? &protocol1:&protocol2;
            protocol_func(recv_buf, &status);
        }
        printf("Waiting for a message\n");
    }
    return 0;
}

int timeout_setup(int socket, struct timeval timeout) {
    if (setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0) {
        fprintf(stderr, "ERROR: setsockopt(recv) failed\n");
        return 0;
    }
    if (setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
               sizeof(timeout)) < 0) {
        fprintf(stderr, "ERROR: setsockopt(send) failed\n");
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

    timeout_setup(status->udp_sock, timeout);

    // giant mess of conditionals
    fprintf(stdout, "\tversion 1 protocol\n");
    int error = 0;
    header msg_header = extract_header(recv_buf);
    if (status->connected == 0) {
        if (check_pass(&msg_header, 0)) {
            if (get_command(&msg_header) == CONNECT) {
                printf("\tReplying with password\n");
                status->connected = 1;
                msg_header.data[1] = status->password;
                insert_header(recv_buf, msg_header);
                if (udp_send(recv_buf, status) < 0) {
                    fprintf(stderr, "sendto()\n");
                }
            } else {
                fprintf(stderr, "ERROR: Unexpected Command\n");
            }
        } else {
            fprintf(stderr, "ERROR: Incorrect Password\n");
        }
    } else if (status->connected == 1) {
        if (check_pass(&msg_header, status->password)) {
            if (get_command(&msg_header) == CONNECT) {
                status->connected = 2;
                timeout_setup(status->udp_sock, timeout_0);
                printf("\tConnected to a client\n");
            } else {
                fprintf(stderr, "ERROR: Unexpected Command\n");
            }
        } else {
            fprintf(stderr, "ERROR: Incorrect Password\n");
        }
    } else {
        if (!check_pass(&msg_header, status->password)) {
            fprintf(stderr, "ERROR: Incorrect Password\n");
        } else {
            switch (get_command(&msg_header)) {
                case CONNECT:
                    fprintf(stderr, "ERROR: Unexpected Command\n");
                    break;
                case QUIT:
                    quit(recv_buf, status);
                    timeout_setup(status->udp_sock, timeout_0);
                    break;
                default:
                    error = request_command(recv_buf, status);
                    break;
            }
        }
    }
    if (error == -2) {
        fprintf(stderr, "ERROR: Invalid client request\n");
    } else if (error == -1) {
        fprintf(stderr, "ERROR: Problem with http response\n");
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
    printf("Data:     %d\n", msg_header.data[UP_REQUEST_DATA]);
    printf("Offset:   %d\n", msg_header.data[UP_BYTE_OFFSET]);
    printf("Total:    %d\n", msg_header.data[UP_TOTAL_SIZE]);
    printf("Payload:  %d\n", msg_header.data[UP_PAYLOAD_SIZE]);
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
    printf("\tBreaking connection\n");
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
    unsigned char http_message[1000];      // used to hold http message from robot
    buffer* http_data = create_buffer(BUFFER_LEN);
    int n;
    header request_header;   // header from the client
    buffer* response;        // buffer to send to client
    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    printf("\tProcessing request\n");

    // acknowledgement of command to client
    udp_send(recv_buf, status);

    // create the http request
    memset(http_message, '\0', 1000);
    request_header = extract_header(recv_buf);
    switch (request_header.data[UP_CLIENT_REQUEST]) {
        case IMAGE:
            printf("\t\tContacting image port\n");
            snprintf((char*)http_message, 100, "GET /snapshot?topic=/robot_%d/image?width=600?height=500 HTTP/1.1\r\n\r\n", status->r_stat.id);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, IMAGE_PORT);
            break;
        case GPS:
            printf("\t\tContacting gps port\n");
            snprintf((char*)http_message, 100, "GET /state?id=%s HTTP/1.1\r\n\r\n", status->r_stat.name);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, GPS_PORT);
            break;
        case LASERS:
            printf("\t\tContacting lasers port\n");
            snprintf((char*)http_message, 100, "GET /state?id=%s HTTP/1.1\r\n\r\n", status->r_stat.name);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, LASERS_PORT);
            break;
        case dGPS:
            printf("\t\tContacting dgps port\n");
            snprintf((char*)http_message, 100, "GET /state?id=%s HTTP/1.1\r\n\r\n", status->r_stat.name);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, dGPS_PORT);
            break;
        case MOVE:
            printf("\t\tContacting move port\n");
            snprintf((char*)http_message, 100, "GET /twist?id=%s&lx=%d HTTP/1.1\r\n\r\n", status->r_stat.name, request_header.data[UP_REQUEST_DATA]);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, GPS_PORT);
            break;
        case TURN:
            printf("\t\tContacting turn port\n");
            snprintf((char*)http_message, 100, "GET /twist?id=%s&az=%d HTTP/1.1\r\n\r\n", status->r_stat.name, request_header.data[UP_REQUEST_DATA]);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, GPS_PORT);
            break;
        case STOP:
            printf("\t\tContacting stop port\n");
            snprintf((char*)http_message, 100, "GET /twist?id=%s&lx=0 HTTP/1.1\r\n\r\n", status->r_stat.name);
            status->r_stat.http_sock = tcp_connect(status->r_stat.hostname, GPS_PORT);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid client request\n");
            return -2;
            break;
    }

    // send the http request
    printf("\t\tWriting request to server\n");
    timeout_setup(status->r_stat.http_sock, timeout);
    write(status->r_stat.http_sock, (char*)http_message, strlen((char*)http_message));

    // read http message into a buffer
    printf("\t\tReceiving reply from server\n");
    memset(http_message, '\0', 1000);
    while ((n = read(status->r_stat.http_sock, (char*)http_message, 1000)) > 0) {
        printf("\t\t\tReceived %d bytes\n", n);
        append_buffer(http_data, (unsigned char*)http_message, n);
        memset(http_message, '\0', 1000);
    }
    printf("\t\tTotal bytes received: %d\n", http_data->len);


    // see what we got
    printf("\t\tAssembled Message:\n\n%s\n", http_data->data);

    // check for '200 OK'
    char ret_code[4];
    memcpy(ret_code, http_data->data + 9, 3); // HTTP/1.1 <ret_code> ~~~~
    ret_code[3] = '\0';
    printf("\t\tReturn code: %s\n", ret_code);
    if (atoi(ret_code) != 200) {
        fprintf(stderr, "ERROR: bad http request\n");
        response = create_message(0, status->password,
                HTTP_ERROR, 0, 0, 0, 0);
        udp_send(response, status);
        return 0;
    }

    // send it to client
    int http_header_len = http_get_data(http_data->data) - http_data->data;
    int a = 0;
    while ((a + http_header_len) < http_data->len) {
        response = create_message(request_header.data[UP_VERSION],
                request_header.data[UP_IDENTIFIER],
                request_header.data[UP_CLIENT_REQUEST],
                request_header.data[UP_REQUEST_DATA],
                a,
                (http_data->len - http_header_len),
                ((http_data->len - (a + http_header_len)) > UP_MAX_PAYLOAD ? UP_MAX_PAYLOAD:(http_data->len - (a + http_header_len))));

        append_buffer(response, http_data->data + a + http_header_len, UP_MAX_PAYLOAD);
        printf("\n\t\tAssembled packet:\n");
        print_header(response);
        printf("%s\n", response->data + UP_HEADER_LEN);
        printf("\t\tSending packet\n");
        udp_send(response, status);
        printf("\t\tPacket sent\n");
        delete_buffer(response);
        a += 272;
    }
    printf("\t\tRequest complete!\n");
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
    fprintf(stderr, "ERROR: http_get_content_length()\n");
    return -1;
}

/* unsigned char* http_get_data
 *   char* http_msg - array containing an http response from the robot
 * Returns a pointer to the beginning of the data section of the http_message
 */
unsigned char* http_get_data(unsigned char* http_msg) {
    char *t = NULL;
    if ((t = strstr((char*)http_msg, "\r\n\r\n")) != NULL) {
        t += strlen("\r\n\r\n");
        return (unsigned char*)t;
    }
    return NULL;
}
