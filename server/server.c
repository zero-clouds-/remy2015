#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"
#include <time.h>
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

typedef struct serverstatus_info {
    struct sockaddr_in cliaddr;
    socklen_t size;
    uint16_t password;
    int connected;
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

void protocol1(buffer* recv_buffer, server_stat status);
void protocol2(buffer* recv_buffer, server_stat status);
int check_version(const header* msg_header);
int check_pass(const header* msg_header, uint32_t pass);
uint32_t get_command(const header* msg_header);


int main(int argc, char** argv) {
    int i, id;
    int http_sock[4], udp_sock;
    char *port, hostname[BUFFER_LEN];
    hostname[0] = '\0';
    if (argc < 4) {
        fprintf(stderr, "usage: %s robot_id http_hostname udp_port\n", argv[0]);
        return -1;
    }
    // read in the required arguments
    id = atoi(argv[1]);
    strncpy(hostname, argv[2], BUFFER_LEN - 1);
    port = argv[3];

    // set up the sockets
    http_sock[1] = tcp_connect(hostname, IMAGE_PORT);
    http_sock[2] = tcp_connect(hostname, GPS_PORT);
    http_sock[3] = tcp_connect(hostname, LASERS_PORT);
    http_sock[4] = tcp_connect(hostname, dGPS_PORT);
    udp_sock = udp_server(port);

    // execution loop
    srand(time(NULL));
    server_stat status;
    status.size = sizeof(status.cliaddr);
    status.password = rand();
    status.connected = 0;
    buffer* recv_buf = create_buffer(BUFFER_LEN);
    for (;;) {

        unsigned char temp[BUFFER_LEN];
        if (recvfrom(udp_sock, temp, BUFFER_LEN, 0,
                    (struct sockaddr *)(&status.cliaddr), &status.size) < 0) {
            fprintf(stderr, "recvfrom()\n");
        }

        // decipher message
        append_buffer(recv_buf, temp, strlen((char*)temp));
        header msg_header = extract_header(recv_buf);
        void (*protocol_func)(buffer*, server_stat);
        protocol_func = check_version(&msg_header) ? &protocol1:&protocol2;
        protocol_func(recv_buf, status);
    }
    return 0;
}

void protocol1(buffer* recv_buf, server_stat status){
    fprintf(stdout, "version 1 protocol\n");
    header msg_header = extract_header(recv_buf);
    if (!status.connected) {
        if (check_pass(&msg_header, 0)) {
            if (get_command(&msg_header) == CONNECT) {
                printf("reply with password\n");
            } else {
                printf("ignore\n");
            }
        } else {
            printf("ignore\n");
        }
    } else {
        if (!check_pass(&msg_header, status.password)) {
            printf("ignore\n");
        } else {
            switch (get_command(&msg_header)) {
                case CONNECT:
                    printf("ignore\n");
                    break;
                case QUIT:
                    printf("breaking connection\n");
                    break;
                case IMAGE:
                    printf("requesting image\n");
                    break;
                case GPS:
                    printf("requesting gps\n");
                    break;
                case dGPS:
                    printf("requesting dgps\n");
                    break;
                case LASERS:
                    printf("requesting lasers\n");
                    break;
                case MOVE:
                    printf("moving robot\n");
                    break;
                case TURN:
                    printf("turning robot\n");
                    break;
                case STOP:
                    printf("stopping robot\n");
                    break;
                default:
                    printf("invalid command\n");
                    break;
            }
        }
    }       
}

void protocol2(buffer* recv_buffer, server_stat status) {
    fprintf(stdout, "version 2 protocol\n");
}

int check_version(const header* msg_header) {
    return msg_header->data[0] == 0;
}

int check_pass(const header* msg_header, uint32_t pass) {
    return msg_header->data[1] == pass;
}

uint32_t get_command(const header* msg_header) {
    return msg_header->data[2];
}
