#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 5000 /* [AI] I was runnning into a lot of issues of not being able to get the server to run, so when i showed \
                       AI my code it said, if I change the port number from 1004 to 5000 I should be able to run my server*/
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct _USR
{
    int clisockfd;     /* socket file descriptor */
    struct _USR *next; /*for linked list queue */

    /*Username and color*/
    char name[50];
    char color[10];
    struct sockaddr_in addr;
} USR;

USR *head = NULL;
USR *tail = NULL;

/* [AI] array of colors which the actual coded names of I recieved from AI */
const char *colors[] = {
    "\033[1;31m", "\033[1;32m", "\033[1;33m",
    "\033[1;34m", "\033[1;35m", "\033[1;36m",
    "\033[0;91m", "\033[0;92m", "\033[0;93m",
    "\033[0;94m"};
int color_index = 0;

void add_tail(int newclisockfd, struct sockaddr_in cli_addr)
{
    if (head == NULL)
    {
        head = (USR *)malloc(sizeof(USR));
        head->clisockfd = newclisockfd;
        head->next = NULL;
        head->addr = cli_addr;
        snprintf(head->color, sizeof(head->color), "%s", colors[color_index++ % 10]); /*[AI] When given the skeleton code and the objective of
                                                                                        assigning a color to a user, it recommeneded I use
                                                                                        this line of code shown to rotate through a list of
                                                                                        ANSI color codes */
        tail = head;
    }
    else
    {
        tail->next = (USR *)malloc(sizeof(USR));
        tail->next->clisockfd = newclisockfd;
        tail->next->next = NULL;
        tail->next->addr = cli_addr;
        snprintf(tail->next->color, sizeof(tail->next->color), "%s", colors[color_index++ % 10]); /* [AI] same logic and structure that was
                                                                                                    provided by AI as before */
        tail = tail->next;
    }
}

void broadcast(int fromfd, char *message)
{
    struct sockaddr_in cliaddr;
    socklen_t clen = sizeof(cliaddr);
    if (getpeername(fromfd, (struct sockaddr *)&cliaddr, &clen) < 0)
        error("ERROR Unknown sender!");

    USR *cur = head;
    while (cur != NULL)
    {
        if (cur->clisockfd != fromfd)
        {
            char buffer[512];

            USR *sender = head;
            while (sender != NULL && sender->clisockfd != fromfd)
                sender = sender->next;

            /*[AI] when speaking with the AI, I brought up the question "How could I go about possibly structuring my broadcast function
            so that each message sent to other clients includes the sender's username in color. And for the condition of nothing being sent
            it should fall back to the IP address" and it recommended the following if-else statement, which seems to work good*/
            if (sender && strlen(sender->name) > 0)
            {
                snprintf(buffer, sizeof(buffer), "%s[%s] %s\033[0m", sender->color, sender->name, message);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), "[%s]:%s", inet_ntoa(cliaddr.sin_addr), message);
            }

            int nmsg = strlen(buffer);
            int nsen = send(cur->clisockfd, buffer, nmsg, 0);
            if (nsen != nmsg)
                error("ERROR send() failed");
        }
        cur = cur->next;
    }
}

typedef struct _ThreadArgs
{
    int clisockfd;
} ThreadArgs;

void *thread_main(void *args)
{
    pthread_detach(pthread_self());

    int clisockfd = ((ThreadArgs *)args)->clisockfd;
    free(args);

    char buffer[256];
    int nrcv;

    /*Finding the user*/
    USR *self = head;
    while (self != NULL && self->clisockfd != clisockfd)
        self = self->next;

    /*[AI] I gave the skeleton code to the AI and simply asked "How can you recieve the client's username right after they connect
    and store it?" When asked this  it gave me the recommendation of using recv() to recieve the username from the socket and store it in the
    USR struct that was proivided. it stressed to me to aslo use memset to clear the buffer to avoid bad data*/
    memset(self->name, 0, sizeof(self->name));
    nrcv = recv(clisockfd, self->name, sizeof(self->name) - 1, 0);

    /*checking return value for quick handling*/
    if (nrcv <= 0)
    {
        close(clisockfd);
        return NULL;
    }

    /*[AI] During the coding process I was stumped at how I could send a message to all connected clients when a new user joins
      It said exactly, "You can generate a formatted join message using snprintf with ANSI color codes and the clients stored username and IP."
      and then showed me an example of a line of code structered like lines 125 and 126*/
    char joinmsg[256];
    snprintf(joinmsg, sizeof(joinmsg), "%s%s (%s) joined the chat room!\n\033[0m",
             self->color, self->name, inet_ntoa(self->addr.sin_addr));
    printf("%s", joinmsg);
    broadcast(clisockfd, joinmsg);

    nrcv = recv(clisockfd, buffer, 255, 0);
    if (nrcv < 0)
        error("ERROR recv() failed");

    while (nrcv > 0)
    {
        broadcast(clisockfd, buffer);

        nrcv = recv(clisockfd, buffer, 255, 0);
        if (nrcv < 0)
            error("ERROR recv() failed");
    }

    /* Message for someone leaving the chat, building off the logic that the AI provided me before in the ANNOUNCE JOIN section, which
       made this part much easier*/
    char leavemsg[256];
    snprintf(leavemsg, sizeof(leavemsg), "%s%s (%s) left the room!\n\033[0m",
             self->color, self->name, inet_ntoa(self->addr.sin_addr));
    printf("%s", leavemsg);
    broadcast(clisockfd, leavemsg);

    close(clisockfd);
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
        add_tail(newsockfd, cli_addr); /*[AI] when trouble shooting my code, AI pointed out that if I left the cli_addr
                                        out of this, I could possibly run into problems of not being able to store the
                                        correct IP address or crash, so I added cli_addr as per its recommendation*/
        ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
        if (args == NULL)
            error("ERROR creating thread argument");

        args->clisockfd = newsockfd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, thread_main, (void *)args) != 0)
            error("ERROR creating a new thread");
    }

    return 0;
}
