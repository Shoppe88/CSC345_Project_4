// CLIENT

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#define PORT_NUM 5000

volatile int keep_running = 1;

void handle_sigint(int sig)
{
    keep_running = 0; // [AI] Graceful shutdown on Ctrl+C
}

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

typedef struct _ThreadArgs
{
    int clisockfd;
} ThreadArgs;

void *thread_main_recv(void *args)
{
    pthread_detach(pthread_self());
    int sockfd = ((ThreadArgs *)args)->clisockfd;
    free(args);

    char buffer[512];
    int n;

    while (keep_running && (n = recv(sockfd, buffer, 512, 0)) > 0)
    {
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }

    if (n == 0)
    {
        printf("Server closed the connection.\n");
        exit(0);
    }
    if (n < 0)
        error("ERROR recv() failed");

    return NULL;
}

void *thread_main_send(void *args)
{
    pthread_detach(pthread_self());
    int sockfd = ((ThreadArgs *)args)->clisockfd;
    free(args);

    char buffer[256];
    int n;

    while (keep_running)
    {
        memset(buffer, 0, 256);
        if (fgets(buffer, 255, stdin) == NULL)
        {
            shutdown(sockfd, SHUT_WR);
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0)
        {
            printf("Empty input detected. Do you want to leave the chat? (y/n): ");
            char choice;
            scanf(" %c", &choice);
            getchar(); // [AI] Clear input buffer
            if (choice == 'y' || choice == 'Y')
            {
                printf("Leaving chat.\n");
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                exit(0);
            }
            continue;
        }

        n = send(sockfd, buffer, strlen(buffer), 0);
        if (n < 0)
            error("ERROR writing to socket");
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint); // [AI] Catch Ctrl+C for graceful shutdown

    if (argc < 2)
    {
        printf("Usage: ./main_client_cp3 <server-ip> [room#|new]\n");
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct sockaddr_in serv_addr;
    socklen_t slen = sizeof(serv_addr);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(PORT_NUM);

    printf("Connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

    if (connect(sockfd, (struct sockaddr *)&serv_addr, slen) < 0)
        error("ERROR connecting");

    // [AI] Handle room selection
    if (argc == 2)
    {
        // No room specified, request available rooms from server
        char msg[] = "ROOM_REQUEST_LIST";
        send(sockfd, msg, strlen(msg), 0);

        char response[1024];
        memset(response, 0, sizeof(response));
        recv(sockfd, response, sizeof(response) - 1, 0);
        printf("%s", response); // Display list of rooms

        char choice[10];
        printf("Choose the room number or type [new] to create a new room: ");
        fgets(choice, sizeof(choice), stdin);
        choice[strcspn(choice, "\n")] = '\0';

        if (strcmp(choice, "new") == 0)
        {
            char create_msg[] = "ROOM_REQUEST_NEW";
            send(sockfd, create_msg, strlen(create_msg), 0);
        }
        else
        {
            char join_msg[64];
            snprintf(join_msg, sizeof(join_msg), "ROOM_REQUEST_JOIN:%s", choice);
            send(sockfd, join_msg, strlen(join_msg), 0);
        }
    }
    else
    {
        // Room specified via command-line
        if (strcmp(argv[2], "new") == 0)
        {
            char msg[] = "ROOM_REQUEST_NEW";
            send(sockfd, msg, strlen(msg), 0);
        }
        else
        {
            char room_request[64];
            snprintf(room_request, sizeof(room_request), "ROOM_REQUEST_JOIN:%s", argv[2]);
            send(sockfd, room_request, strlen(room_request), 0);
        }
    }

    // [AI] Read server response confirming room connection
    char response[128];
    memset(response, 0, sizeof(response));
    recv(sockfd, response, sizeof(response) - 1, 0);
    printf("%s\n", response);

    if (strstr(response, "Invalid") != NULL)
    {
        printf("Failed to join room. Exiting.\n");
        close(sockfd);
        exit(1);
    }

    // [AI] Prompt for username
    char username[50];
    do
    {
        printf("Type your user name: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = '\0';
    } while (strlen(username) == 0);

    send(sockfd, username, strlen(username), 0);

    printf("%s (%s) joined the chat room!\n", username, inet_ntoa(serv_addr.sin_addr));

    pthread_t tid1, tid2;
    ThreadArgs *args;

    args = malloc(sizeof(ThreadArgs));
    args->clisockfd = sockfd;
    pthread_create(&tid1, NULL, thread_main_send, (void *)args);

    args = malloc(sizeof(ThreadArgs));
    args->clisockfd = sockfd;
    pthread_create(&tid2, NULL, thread_main_recv, (void *)args);

    pthread_join(tid1, NULL);

    close(sockfd);
    return 0;
}
