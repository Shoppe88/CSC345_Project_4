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
#include <signal.h> /*[AI] For signal handling to prevent abrupt termination*/

#define PORT_NUM 5000 /* [AI] was running into issues running the server, ai reccomemded I increase the port number when I brought up the problem*/

/*[AI] When I would press Ctrl+C to exit the client or when the server disconnects, my threads kept on crashing unpredictably. So I wanted
an optimal way I can control the shutdown safely, so it recommended I use "a shared gloabal flag" like below on line 17 */
volatile int keep_running = 1;

void handle_sigint(int sig)
{
    keep_running = 0; /*[AI] suggested handler function that ties into the AI use below on line 91*/
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

    /*[AI] Due to some odd behavior fromm the terminal after recieving messages like, cutting off, corruption, or other garbage characters, the AI
    recomended to use the followig while loop which, inside has line 44, which makes sure to null-terminate the buffer, to guarentee that the
    string ends where it is supposed to*/
    while (keep_running && (n = recv(sockfd, buffer, 512, 0)) > 0)
    {
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }

    if (n == 0)
    {
        printf("Server closed the connection.\n"); /*informing user of shut down*/
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
            shutdown(sockfd, SHUT_WR); /*[AI] recommendation*/
        }

        buffer[strcspn(buffer, "\n")] = '\0'; /*Trimming newline*/

        /*message for someone leaving the chat*/
        if (strlen(buffer) == 0)
        {
            printf("Empty input detected. Leaving chat.\n");
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            exit(0); /*[AI] suggested that I exit immediately due to some server crashing issues I was runnning into*/
        }

        n = send(sockfd, buffer, strlen(buffer), 0);
        if (n < 0)
            error("ERROR writing to socket");
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint); /*[AI] I was also running into a issue of when using Ctrl+C, the program would instantly dissconect and wouldn't have time to send
                                     any leaving message, so to counter that, it suggested I catch the SIGINT using signal(), and use the signal handler function
                                     it recommended above: handle_sigint*/

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

    int status = connect(sockfd, (struct sockaddr *)&serv_addr, slen);
    if (status < 0)
        error("ERROR connecting");

    char username[50]; /*[AI] I asked ai what a safe number I could use here would be, it gave me the option of 50
                         it explained that it provides a very good upper bound for any name while not taking up
                         much memory - which is why i used 50*/
    do
    {
        printf("Type your user name: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = '\0';
    } while (strlen(username) == 0); // [AI] Prevent sending empty username

    send(sockfd, username, strlen(username), 0);

    printf("%s (%s) joined the chat room!\n", username, argv[1]);

    pthread_t tid1;
    pthread_t tid2;

    ThreadArgs *args;

    /*[AI] I started to run into issues with my code closing immediately after one message, and some clients were disconnecting when a user would
    send a message, so when given my skeleton code and told about the problems I was running into, it suggested I went with this approach below
    suggesting I use two seperate threads for input and output which will avoid any blocks that were occuring in my code.*/
    args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
    args->clisockfd = sockfd;
    pthread_create(&tid1, NULL, thread_main_send, (void *)args);

    args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
    args->clisockfd = sockfd;
    pthread_create(&tid2, NULL, thread_main_recv, (void *)args);

    pthread_join(tid1, NULL);
    /*----------------------------------------------------------------------------------------------------*/
    close(sockfd);

    return 0;
}
