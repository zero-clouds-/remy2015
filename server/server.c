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

// valid command encodings
#define CONNECT     0
#define QUIT        255
#define IMAGE       2
#define GPS         4
#define dGPS        8
#define LASERS      16
#define MOVE        32
#define TURN        64
#define STOP        128

enum http_port {image, gps, lasers, dgps};

typedef struct robot_info {
    int id;
    int http_sock[4]; // 0 = image, 1 = GPS/move/turn/stop, 2 = Lasers, 3 = dGPS
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

void protocol1(buffer* recv_buffer, server_stat* status);
void protocol2(buffer* recv_buffer, server_stat* status);
int check_version(const header* msg_header);
int check_pass(const header* msg_header, uint32_t pass);
uint32_t get_command(const header* msg_header);
ssize_t udp_send(buffer* recv_buffer, server_stat* status);
void quit(buffer* recv_buf, server_stat* status);
void request_image(buffer* recv_buf, server_stat* status);
void request_gps(buffer* recv_buf, server_stat* status);
void request_lasers(buffer* recv_buf, server_stat* status);
void request_dgps(buffer* recv_buf, server_stat* status);
void request_move(buffer* recv_buf, header msg_header, server_stat* status);
void request_turn(buffer* recv_buf, header msg_header, server_stat* status);
void request_stop(buffer* recv_buf, server_stat* status);

int main(int argc, char** argv) {
    int i;
    server_stat status;
    char *port, hostname[BUFFER_LEN];
    hostname[0] = '\0';
    if (argc < 4) {
        fprintf(stderr, "usage: %s robot_id http_hostname udp_port\n", argv[0]);
        return -1;
    }

    // read in the required arguments
    status.r_stat.id = atoi(argv[1]);
    strncpy(hostname, argv[2], BUFFER_LEN - 1);
    port = argv[3];

    // set up the sockets
    status.r_stat.http_sock[1] = tcp_connect(hostname, IMAGE_PORT);
    status.r_stat.http_sock[2] = tcp_connect(hostname, GPS_PORT);
    status.r_stat.http_sock[3] = tcp_connect(hostname, LASERS_PORT);
    status.r_stat.http_sock[4] = tcp_connect(hostname, dGPS_PORT);
    status.udp_sock = udp_server(port);

    // execution loop
    srand(time(NULL));
    status.size = sizeof(status.cliaddr);
    status.password = rand();
    status.connected = 0;
    buffer* recv_buf = create_buffer(BUFFER_LEN);
    for (;;) {

        unsigned char temp[BUFFER_LEN];
        if (recvfrom(status.udp_sock, temp, BUFFER_LEN, 0,
                    (struct sockaddr *)(&status.cliaddr), &status.size) < 0) {
            fprintf(stderr, "recvfrom()\n");
        }

        // decipher message
        append_buffer(recv_buf, temp, strlen((char*)temp));
        header msg_header = extract_header(recv_buf);

        // send to correct protocol
        void (*protocol_func)(buffer*, server_stat*);
        protocol_func = check_version(&msg_header) ? &protocol1:&protocol2;
        protocol_func(recv_buf, &status);
    }
    return 0;
}

/* void protocol1
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 * Processes the recv_buf as the class protocol dictates
 */
void protocol1(buffer* recv_buf, server_stat* status){
    // giant mess of conditionals
    fprintf(stdout, "version 1 protocol\n");
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
                printf("ignore\n");
            }
        } else {
            printf("ignore\n");
        }
    } else if (status->connected == 1) {
        if (check_pass(&msg_header, status->password)) {
            if (get_command(&msg_header) == CONNECT) {
                status->connected = 2;
            } else {
                printf("ignore\n");
            }
        } else {
            printf("ignore\n");
        }
    } else {
        if (!check_pass(&msg_header, status->password)) {
            printf("ignore\n");
        } else {
            switch (get_command(&msg_header)) {
                case CONNECT:
                    printf("ignore\n");
                    break;
                case QUIT:
                    quit(recv_buf, status);
                    break;
                case IMAGE:
                    request_image(recv_buf, status);
                    break;
                case GPS:
                    request_gps(recv_buf, status);
                    break;
                case dGPS:
                    request_dgps(recv_buf, status);
                    break;
                case LASERS:
                    request_lasers(recv_buf, status);
                    break;
                case MOVE:
                    request_move(recv_buf, msg_header, status);
                    break;
                case TURN:
                    request_turn(recv_buf, msg_header, status);
                    break;
                case STOP:
                    request_stop(recv_buf, status);
                    break;
                default:
                    printf("invalid command\n");
                    break;
            }
        }
    }       
}

/* void protocol2
 *   buffer* recv_buffer - buffer containing the message sent by the client to the proxy
 *   server_stat* status - status and general information from the server
 */
void protocol2(buffer* recv_buffer, server_stat* status) {
    fprintf(stdout, "version 2 protocol\n");
}

/* int check_version
 *   const header* msg_header - protocol header
 * Returns the version identifier of the client message
 */
int check_version(const header* msg_header) {
    return msg_header->data[UP_IDENTIFIER] == 0;
}

/* int check_pass
 *   const header* msg_header - protocol header
 * Returns 1 if the password in the header matches the server password
 * Returns 0 otherwise
 */
int check_pass(const header* msg_header, uint32_t pass) {
    return msg_header->data[UP_CLIENT_REQUEST] == pass;
}

/* uint32_t get_command
 *   const header* msg_header - protocol header
 * Returns the command requested by the client
 */
uint32_t get_command(const header* msg_header) {
    return msg_header->data[UP_REQUEST_DATA];
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
 * Requests image from the robot
 * Forwards data from robot to client
 */
void request_image(buffer* recv_buf, server_stat* status) {
    char http_message[1000];
    int n;

    printf("requesting image\n");
    udp_send(recv_buf, status);
    strncpy(http_message, "GET /snapshot?topic=/robot_11/image?width=600?height=500 HTTP/1.1\r\n\r\n", 50);
    write(status->r_stat.http_sock[image], http_message, strlen(http_message));

    n = read(status->r_stat.http_sock[image], http_message, 1000);
    if (n < 0) {
        fprintf(stderr, "read()\n");
    }
    // decipher http message
    // if successful, begin sending to client
    // else failure
}

// not down here yet. all of this will probably dissapear into a general request_command function
void request_gps(buffer* recv_buf, server_stat* status) {
    char http_message[50];
    printf("requesting gps\n");
    udp_send(recv_buf, status);
    strncpy(http_message, "GET /state?id=abrogate2 HTTP/1.1\r\n\r\n", 50);
    write(status->r_stat.http_sock[1], http_message, strlen(http_message));
}

void request_lasers(buffer* recv_buf, server_stat* status) {
    char http_message[50];
    printf("requesting lasers\n");
    udp_send(recv_buf, status);
    strncpy(http_message, "GET /state?id=abrogate2 HTTP/1.1\r\n\r\n", 50);
    write(status->r_stat.http_sock[2], http_message, strlen(http_message));
}

void request_dgps(buffer* recv_buf, server_stat* status) {
    char http_message[50];
    printf("requesting dgps\n");
    udp_send(recv_buf, status);
    strncpy(http_message, "GET /state?id=abrogate2 HTTP/1.1\r\n\r\n", 50);
    write(status->r_stat.http_sock[3], http_message, strlen(http_message));
}

void request_move(buffer* recv_buf, header msg_header, server_stat* status) {
    char http_message[50];
    printf("moving robot\n");
    if (msg_header.data[UP_REQUEST_DATA] > 5 || msg_header.data[UP_REQUEST_DATA] < -5) {
        printf("invalid command\n");
        return;
    }
    udp_send(recv_buf, status);
    snprintf(http_message, 50, "GET /state?id=abrogate2&lx=%d HTTP/1.1\r\n\r\n", msg_header.data[3]);
    write(status->r_stat.http_sock[1], http_message, strlen(http_message));
}

void request_turn(buffer* recv_buf, header msg_header, server_stat* status) {
    char http_message[50];
    printf("turning robot\n");
    if (msg_header.data[3] > 1 || msg_header.data[3] < -1) {
        printf("invalid command\n");
        return;
    }
    udp_send(recv_buf, status);
    snprintf(http_message, 50, "GET /state?id=abrogate2&az=%d HTTP/1.1\r\n\r\n", msg_header.data[3]);
    write(status->r_stat.http_sock[1], http_message, strlen(http_message));
}

void request_stop(buffer* recv_buf, server_stat* status) {
    char http_message[50];
    printf("stopping robot\n");
    udp_send(recv_buf, status);
    strncpy(http_message, "GET /state?id=abrogate2&lx=0 HTTP/1.1\r\n\r\n", 50);
    write(status->r_stat.http_sock[1], http_message, strlen(http_message));
}
