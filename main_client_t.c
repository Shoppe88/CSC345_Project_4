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

#define PORT_NUM 5000 /* [AI] was running into issues running the server, ai reccomemded I increase the port number when I brought up the problem*/

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

    /*keep receiving and displaying message from server*/
    char buffer[512];
    int n;

    n = recv(sockfd, buffer, 512, 0);
    while (n > 0)
    {
        memset(buffer, 0, 512);
        n = recv(sockfd, buffer, 512, 0);
        if (n < 0)
            error("ERROR recv() failed");

        printf("%s\n", buffer);
    }

    return NULL;
}

void *thread_main_send(void *args)
{
    pthread_detach(pthread_self());

    int sockfd = ((ThreadArgs *)args)->clisockfd;
    free(args);

    /*keep sending messages to the server*/
    char buffer[256];
    int n;

    while (1)
    {
        memset(buffer, 0, 256);
        fgets(buffer, 255, stdin);

        if (strlen(buffer) == 1)
            buffer[0] = '\0';

        n = send(sockfd, buffer, strlen(buffer), 0);
        if (n < 0)
            error("ERROR writing to socket");

        if (n == 0)
            break;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        error("Please speicify hostname");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct sockaddr_in serv_addr;
    socklen_t slen = sizeof(serv_addr);
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(PORT_NUM);

    printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

    int status = connect(sockfd,
                         (struct sockaddr *)&serv_addr, slen);
    if (status < 0)
        error("ERROR connecting");

    /* Prompt for username and send to server*/
    char username[50]; /*[AI] I asked ai what a safe number I could use here would be, it gave me the option of 50
                        it explained that it provides a very good upper bound for any name while not taking up
                        much memory - which is why i used 50*/
    printf("Type your user name: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0'; /*remove newline*/
    send(sockfd, username, strlen(username), 0);

    /* Printing simple join confirmation */
    printf("%s (%s) joined the chat room!\n", username, argv[1]);

    pthread_t tid1;
    pthread_t tid2;

    ThreadArgs *args;

    args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
    args->clisockfd = sockfd;
    pthread_create(&tid1, NULL, thread_main_send, (void *)args);

    args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
    args->clisockfd = sockfd;
    pthread_create(&tid2, NULL, thread_main_recv, (void *)args);

    pthread_join(tid1, NULL);

    close(sockfd);

    return 0;
}
