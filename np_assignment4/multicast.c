#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_MSG_SIZE 1024
#define BASE_PORT 5000
#define BASE_MULTICAST_ADDR "239.0.0."

void child_process(int child_num, int num_children, pid_t *pids) {
    // Store my PID
    pids[child_num] = getpid();

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Allow multiple sockets to use the same port
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR failed");
        exit(1);
    }

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(BASE_PORT + (getpid() % 1000));

    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Set multicast options
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr("127.0.0.1");  // Use loopback interface
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &localInterface,
                   sizeof(localInterface)) < 0) {
        perror("Setting local interface failed");
        exit(1);
    }

    // Join multicast group
    char group_addr[16];
    sprintf(group_addr, "%s%d", BASE_MULTICAST_ADDR, getpid() % 256);
    printf("Child %d joining group %s\n", getpid(), group_addr);

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group_addr);
    mreq.imr_interface.s_addr =
        inet_addr("127.0.0.1");  // Use loopback interface

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
        0) {
        perror("Adding to multicast group failed");
        exit(1);
    }

    // Set multicast TTL
    unsigned char ttl = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("Setting TTL failed");
        exit(1);
    }

    // Enable loopback so we can receive our own messages
    unsigned char loop = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) <
        0) {
        perror("Setting loopback mode failed");
        exit(1);
    }

    printf("Child PID %d is ready\n", getpid());

    // Wait for all processes to be ready
    sleep(2);

    int message_count = 0;

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    while (1) {  // Infinite loop
        // Try to receive ALL pending messages first
        struct timeval tv;
        tv.tv_sec = 1;   // Increase to 1 second timeout
        tv.tv_usec = 0;  // 0 microseconds

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            perror("Error setting timeout");
            exit(1);
        }

        while (1) {  // Keep receiving until no more messages
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            char recv_message[MAX_MSG_SIZE];

            ssize_t recv_len =
                recvfrom(sock, recv_message, MAX_MSG_SIZE - 1, 0,
                         (struct sockaddr *)&sender_addr, &sender_len);

            if (recv_len < 0) {
                if (errno != EAGAIN || errno != EWOULDBLOCK) {
                    perror("recvfrom failed");
                }
                break;
            }

            recv_message[recv_len] = '\0';
            printf("Child PID %d received on FD %d from %s:%d: %s\n", getpid(),
                   sock, inet_ntoa(sender_addr.sin_addr),
                   ntohs(sender_addr.sin_port), recv_message);
        }

        // Now send messages to all other groups
        char message[MAX_MSG_SIZE];
        sprintf(message, "Message %d from PID %d", message_count++, getpid());

        // Send to each child's multicast group except my own
        for (int i = 0; i < num_children; i++) {
            if (pids[i] == getpid()) continue;  // Skip my own group

            struct sockaddr_in dest_addr;
            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(BASE_PORT + (pids[i] % 1000));

            char dest_group_addr[16];
            sprintf(dest_group_addr, "%s%d", BASE_MULTICAST_ADDR,
                    pids[i] % 256);
            dest_addr.sin_addr.s_addr = inet_addr(dest_group_addr);

            sendto(sock, message, strlen(message), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }
        // Increase delay to make output more readable
        sleep(1);  // 1 second delay
    }

    close(sock);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_children>\n", argv[0]);
        exit(1);
    }

    int num_children = atoi(argv[1]);
    if (num_children <= 0) {
        fprintf(stderr, "Number of children must be positive\n");
        exit(1);
    }

    // Create shared memory using mmap for PIDs
    pid_t *pids =
        mmap(NULL, num_children * sizeof(pid_t), PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (pids == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    // Create children
    for (int i = 0; i < num_children; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        if (pid == 0) child_process(i, num_children, pids);
    }

    for (int i = 0; i < num_children; i++) wait(NULL);

    // Clean up shared memory
    munmap(pids, num_children * sizeof(pid_t));
    return 0;
}
