#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 1024
void error(const char *m)
{
    perror(m);
    exit(0);
}

static void *recv_thread(void *arg)
{
    int sockfd = *(int *)arg;
    pthread_detach(pthread_self());
    char buf[512];
    int n;
    while ((n = recv(sockfd, buf, 511, 0)) > 0)
    {
        buf[n] = '\0';
        printf("\n%s\n", buf);
    }
    exit(0);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        error("usage: client <server_ip>");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("socket");
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(PORT_NUM);
    printf("Connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("connect");

    /* ---------- handle username + room prompts ---------- */
    char line[64];
    int n = recv(sockfd, line, 63, 0);
    if (n <= 0)
        error("recv");
    line[n] = '\0';
    printf("%s", line);
    fgets(line, 63, stdin);
    send(sockfd, line, strlen(line), 0);
    n = recv(sockfd, line, 63, 0);
    if (n <= 0)
        error("recv");
    line[n] = '\0';
    printf("%s", line);
    fgets(line, 63, stdin);
    send(sockfd, line, strlen(line), 0);

    /* ---------- launch threads ---------- */
    pthread_t rtid;
    int *argfd = malloc(sizeof(int));
    *argfd = sockfd;
    pthread_create(&rtid, NULL, recv_thread, argfd);

    char buf[256];
    while (1)
    {
        fgets(buf, 255, stdin);
        if (strcmp(buf, "\n") == 0)
            buf[0] = '\0';
        send(sockfd, buf, strlen(buf), 0);
        if (buf[0] == '\0')
            break;
    }
    close(sockfd);
    return 0;
}
