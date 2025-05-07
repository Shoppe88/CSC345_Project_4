#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 1004
#define MAX_CLIENTS 100

struct sockaddr_in client_list[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct _ThreadArgs
{
    int clisockfd;
    struct sockaddr_in cli_addr;
} ThreadArgs;

void *thread_main(void *args)
{
    // make sure thread resources are deallocated upon return
    pthread_detach(pthread_self());

    // get socket descriptor from argument
    int clisockfd = ((ThreadArgs *)args)->clisockfd;
    struct sockaddr_in cli_addr = ((ThreadArgs *)args)->cli_addr;
    free(args);

    //-------------------------------
    // Now, we receive/send messages
    char buffer[256];
    int nsen, nrcv;

    nrcv = recv(clisockfd, buffer, 256, 0);
    if (nrcv < 0)
        error("ERROR recv() failed");

    while (nrcv > 0)
    {
        nsen = send(clisockfd, buffer, nrcv, 0);
        if (nsen != nrcv)
            error("ERROR send() failed");

        nrcv = recv(clisockfd, buffer, 256, 0);
        if (nrcv < 0)
            error("ERROR recv() failed");
    }
    close(clisockfd);

    // Remove client from list
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (client_list[i].sin_addr.s_addr == cli_addr.sin_addr.s_addr &&
            client_list[i].sin_port == cli_addr.sin_port)
        {
            // Shift the rest down
            for (int j = i; j < client_count - 1; j++)
            {
                client_list[j] = client_list[j + 1];
            }
            client_count--;
            break;
        }
    }
    printf("=== Connected clients ===\n");
    for (int i = 0; i < client_count; i++)
    {
        printf("%s:%d\n", inet_ntoa(client_list[i].sin_addr), ntohs(client_list[i].sin_port));
    }
    if (client_count == 0)
    {
        printf("(no clients connected)\n");
    }
    printf("=========================\n");
    pthread_mutex_unlock(&client_list_mutex);

    return NULL;
}

int main(int argc, char *argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct sockaddr_in serv_addr;
    socklen_t slen = sizeof(serv_addr);
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_NUM);

    int status = bind(sockfd,
                      (struct sockaddr *)&serv_addr, slen);
    if (status < 0)
        error("ERROR on binding");

    listen(sockfd, 5);

    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t clen = sizeof(cli_addr);
        int newsockfd = accept(sockfd,
                               (struct sockaddr *)&cli_addr, &clen);
        if (newsockfd < 0)
            error("ERROR on accept");

        printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));

        if (newsockfd < 0)
            error("ERROR on accept");

        // Add client to list
        pthread_mutex_lock(&client_list_mutex);
        if (client_count < MAX_CLIENTS)
        {
            client_list[client_count++] = cli_addr;
        }
        else
        {
            printf("Max client limit reached!\n");
        }
        pthread_mutex_unlock(&client_list_mutex);

        // Print updated client list
        pthread_mutex_lock(&client_list_mutex);
        printf("=== Connected clients ===\n");
        for (int i = 0; i < client_count; i++)
        {
            printf("%s:%d\n", inet_ntoa(client_list[i].sin_addr), ntohs(client_list[i].sin_port));
        }
        if (client_count == 0)
        {
            printf("(no clients connected)\n");
        }
        printf("=========================\n");
        pthread_mutex_unlock(&client_list_mutex);

        // prepare ThreadArgs structure to pass client socket
        ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
        if (args == NULL)
            error("ERROR creating thread argument");

        args->clisockfd = newsockfd;
        args->cli_addr = cli_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, thread_main, (void *)args) != 0)
            error("ERROR creating a new thread");
    }

    return 0;
}
