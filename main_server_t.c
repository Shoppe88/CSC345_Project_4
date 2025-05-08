#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 5000   /* [AI] I was runnning into a lot of issues of not being able to get the server to run, so when i showed \
                     AI my code it said, if I change the port number from 1004 to 5000 I should be able to run my server*/
#define MAX_CLIENTS 100 // [AI] Added to set a limit for client list handling

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER; // [AI] Used for thread-safe client list access

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
    struct sockaddr_in addr; // [AI] Needed for printing client IP and join/leave announcements
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

void print_client_list() // [AI] Used to debug and visualize current connections
{
    printf("=== Connected clients ===\n");
    USR *cur = head;
    while (cur != NULL)
    {
        printf("%s:%d\n", inet_ntoa(cur->addr.sin_addr), ntohs(cur->addr.sin_port));
        cur = cur->next;
    }
    if (head == NULL)
    {
        printf("(no clients connected)\n");
    }
    printf("=========================\n");
}

void add_tail(int newclisockfd, struct sockaddr_in cli_addr)
{
    USR *new_node = (USR *)malloc(sizeof(USR));
    if (!new_node)
        error("ERROR allocating memory for client node");

    new_node->clisockfd = newclisockfd;
    new_node->addr = cli_addr;
    snprintf(new_node->color, sizeof(new_node->color), "%s", colors[color_index++ % 10]); /*[AI] Assign rotating color to each user*/
    new_node->next = NULL;

    if (head == NULL)
    {
        head = tail = new_node;
    }
    else
    {
        tail->next = new_node;
        tail = new_node;
    }
}

void broadcast(int fromfd, char *message)
{
    pthread_mutex_lock(&client_list_mutex);

    struct sockaddr_in cliaddr;
    socklen_t clen = sizeof(cliaddr);
    if (getpeername(fromfd, (struct sockaddr *)&cliaddr, &clen) < 0)
        error("ERROR Unknown sender!");

    USR *sender = head;
    while (sender != NULL && sender->clisockfd != fromfd)
        sender = sender->next;

    USR *cur = head;
    while (cur != NULL)
    {
        if (cur->clisockfd != fromfd)
        {
            char buffer[512];
            if (sender && strlen(sender->name) > 0)
                snprintf(buffer, sizeof(buffer), "%s[%s] %s\033[0m", sender->color, sender->name, message); /*[AI] Username + color formatting*/
            else
                snprintf(buffer, sizeof(buffer), "[%s]:%s", inet_ntoa(cliaddr.sin_addr), message);

            int nmsg = strlen(buffer);
            int nsen = send(cur->clisockfd, buffer, nmsg, 0);
            if (nsen != nmsg)
                perror("ERROR send() failed");
        }
        cur = cur->next;
    }

    pthread_mutex_unlock(&client_list_mutex);
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
    int message_count = 0; /*[AI] Track how many messages a client sends before disconnecting*/

    pthread_mutex_lock(&client_list_mutex);
    USR *self = head;
    while (self != NULL && self->clisockfd != clisockfd)
        self = self->next;
    pthread_mutex_unlock(&client_list_mutex);

    if (self == NULL)
    {
        close(clisockfd);
        return NULL; /*[AI] Prevent crash if client wasn't properly registered*/
    }

    memset(self->name, 0, sizeof(self->name));
    nrcv = recv(clisockfd, self->name, sizeof(self->name) - 1, 0);

    if (nrcv <= 0)
    {
        close(clisockfd);
        return NULL;
    }

    char joinmsg[256];
    snprintf(joinmsg, sizeof(joinmsg), "%s%s (%s) joined the chat room!\n\033[0m",
             self->color, self->name, inet_ntoa(self->addr.sin_addr));
    printf("%s", joinmsg);
    broadcast(clisockfd, joinmsg);

    while ((nrcv = recv(clisockfd, buffer, 255, 0)) > 0)
    {
        buffer[nrcv] = '\0';
        broadcast(clisockfd, buffer);
        message_count++;
    }

    if (nrcv < 0)
        perror("ERROR recv() failed");

    if (message_count > 0)
    {
        char leavemsg[256];
        snprintf(leavemsg, sizeof(leavemsg), "%s%s (%s) left the room!\n\033[0m",
                 self->color, self->name, inet_ntoa(self->addr.sin_addr));
        printf("%s", leavemsg);
        broadcast(clisockfd, leavemsg);
    }

    shutdown(clisockfd, SHUT_RDWR);
    close(clisockfd);

    pthread_mutex_lock(&client_list_mutex);
    USR *prev = NULL, *cur = head;
    while (cur != NULL)
    {
        if (cur->clisockfd == clisockfd)
        {
            if (prev == NULL)
            {
                head = cur->next;
                if (cur == tail)
                    tail = NULL;
            }
            else
            {
                prev->next = cur->next;
                if (cur == tail)
                    tail = prev;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&client_list_mutex);

    pthread_mutex_lock(&client_list_mutex);
    print_client_list();
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

        pthread_mutex_lock(&client_list_mutex);
        add_tail(newsockfd, cli_addr);
        pthread_mutex_unlock(&client_list_mutex);

        pthread_mutex_lock(&client_list_mutex);
        print_client_list();
        pthread_mutex_unlock(&client_list_mutex);

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
