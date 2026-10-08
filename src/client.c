#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <packet.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "E: wrong usage\v%s <ip> <port>\n", argv[0]);
    return 1;
  }
  int port = atoi(argv[2]);
    int fd;
    struct sockaddr_in s_sock;
    s_sock.sin_family = AF_INET;
    s_sock.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], &s_sock.sin_addr);

    fd = socket(AF_INET, SOCK_STREAM, 0);

    int server_fd;
    socklen_t s_len = sizeof(s_sock);
    printf("connecting ...\n");
CONNECT:
    if ((server_fd = connect(fd, (struct sockaddr*)&s_sock, s_len)) < 0) {
        perror("connect");
        close(server_fd);
        goto CONNECT;
    }

    header pheader;
    recv(fd, &pheader, sizeof(pheader), 0);
    uint32_t len = ntohl(pheader.length);
    char *msg = malloc(len);
    recv(fd, msg, len, 0);
    printf("msg[%d]: %s\n", len, msg);

    free(msg);
    close(server_fd);
    close(fd);
    return 0;
}

