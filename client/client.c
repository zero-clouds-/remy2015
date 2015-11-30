#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"
#include <unistd.h>     /* for close() */

#define BUFFER_LEN 512

char * formatMessage() {
    return "a sample message";
}

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

int main(int argc, char** argv) {
        int port, sock;
        char hostname[BUFFER_LEN];
        struct addrinfo* serv_addr;
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


        struct sockaddr_in servAddr;
        struct sockaddr_in clntAddr;
        /* Construct the server address structure */
        memset(&servAddr, 0, sizeof(servAddr));    /* reset structure fields */
        servAddr.sin_family = AF_INET;                 /* Internet address family */
        servAddr.sin_addr.s_addr = inet_addr(hostname);  /* IP address of server*/
        servAddr.sin_port   = htons(port);     /* Server port to send to */

        char *totalMessage = (char *)malloc(BUFFER_LEN * 10);

        //Grace - implement client to receive server response
        for (;;) {

                //configure formatMessage() to generate a proper message to specs
                char * serverMessage = formatMessage();
                if(sendto(sock, serverMessage, strlen(serverMessage), 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
                    //error message
                    error("error with sendto()");
                }

                /* receive a resonse from the server
                 * figure out how much space we need when getting the response
                 * paste each message into the string to write the file later
                 */
                unsigned int clntSize = sizeof(clntAddr);
                char * serverResponse = (char *)malloc(350);
                if(recvfrom(sock, serverResponse, 349, 0, (struct sockaddr *) &clntAddr, &clntSize) < 0) {
                    error("error with recvfrom()");
                }

                //paste payload into total char message
                char * offset = (char *)malloc(4);
                strncpy(offset, serverResponse + 16, 4);
                int offsetNum = atoi(offset);
        
                strcpy(totalMessage + offsetNum, serverResponse + 28);
        }
        close(sock);
        freeaddrinfo(serv_addr);
        return 0;
}
