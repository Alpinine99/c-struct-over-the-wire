#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <packet.h>


int main() {
    int server_fd;
    struct sockaddr_in sock;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sock.sin_family = AF_INET;
    sock.sin_port = htons(8080);
    sock.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&sock, sizeof(sock)) < 0) {
        perror("bind");
        return 1;
    }

    listen(server_fd, 3);
    printf("listening ...\n");

    int client_fd;
    struct sockaddr_in client;
    socklen_t s_len = sizeof(client);
ACCEPT:
    if ((client_fd = accept(server_fd, (struct sockaddr*)&client, &s_len)) < 0) {
        perror("accept");
        goto ACCEPT;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client.sin_port);
    printf("connected to %s:%d\n", client_ip, client_port);
   
    char msg[] = "hello mate, I'm just calling to check on you man. You've been quiet of late what's going on man?";
    packet *pkt;
    size_t plen = sizeof(header) + sizeof(msg);
    pkt = malloc(plen);
    pkt->p_header.length = htonl(sizeof(msg));
    memcpy(pkt->content, msg, sizeof(msg));
    send(client_fd, pkt, plen, 0);

    free(pkt);
    close(client_fd);
    close(server_fd);
    return 0;
}

