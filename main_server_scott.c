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

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct _USR
{
    int clisockfd;
    struct sockaddr_in cli_addr;
    char name[50];
    struct _USR *next;
} USR;

USR *head = NULL;
USR *tail = NULL;

void print_client_list(); // recommended addition
void add_tail(int newclisockfd, struct sockaddr_in cliaddr);
void broadcast(int fromfd, const char *msg);
void *thread_main(void *args);
static USR *find_user(int sock);

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
        pthread_mutex_lock(&client_list_mutex); // AI recommended addition
        add_tail(newsockfd, cli_addr);          // AI recommended addition
        pthread_mutex_unlock(&client_list_mutex);

        printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));

        // prepare USR structure to pass client socket
        USR *args = (USR *)malloc(sizeof(USR));
        if (args == NULL)
            error("ERROR creating thread argument");

        args->clisockfd = newsockfd;
        args->cli_addr = cli_addr; // AI recommended addition

        // Print updated client list
        pthread_mutex_lock(&client_list_mutex);
        print_client_list();
        pthread_mutex_unlock(&client_list_mutex);

        pthread_t tid;
        if (pthread_create(&tid, NULL, thread_main, (void *)args) != 0)
            error("ERROR creating a new thread");
    }

    return 0;
}

void add_tail(int newclisockfd, struct sockaddr_in cliaddr)
{
    USR *new_node = (USR *)malloc(sizeof(USR));
    // error check recommended to handle edge cases
    if (!new_node)
        error("ERROR allocating memory for client node");

    // recommened adding new nodes when a new client
    new_node->clisockfd = newclisockfd;
    new_node->cli_addr = cliaddr;
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

void broadcast(int fromfd, const char *msg)
{
    USR *sender = find_user(fromfd);

    pthread_mutex_lock(&client_list_mutex);
    for (USR *cur = head; cur; cur = cur->next)
    {
        if (cur->clisockfd == fromfd)
            continue;

        char buf[640];
        if (sender && *sender->name)
            snprintf(buf, sizeof buf, "[%s] %s", sender->name, msg);
        else if (sender)
            snprintf(buf, sizeof buf, "[%s] %s",
                     inet_ntoa(sender->cli_addr.sin_addr), msg);
        else
            snprintf(buf, sizeof buf, "[unknown] %s", msg);

        send(cur->clisockfd, buf, strlen(buf), 0);
    }
    pthread_mutex_unlock(&client_list_mutex);
}

void *thread_main(void *arg)
{
    pthread_detach(pthread_self());

    /* unwrap argument and look up our USR record */
    USR *aux = (USR *)arg;
    int clisockfd = aux->clisockfd;
    free(aux);

    USR *self = find_user(clisockfd);
    if (!self)
    {
        close(clisockfd);
        return NULL;
    }

    /* ---------- username handshake ---------- */
    const char ask[] = "Enter username: ";
    send(clisockfd, ask, sizeof(ask) - 1, 0);

    char uname[50] = {0};
    int n = recv(clisockfd, uname, sizeof(uname) - 1, 0);
    if (n <= 0)
    {
        close(clisockfd);
        return NULL;
    }

    /* strip trailing CR/LF and store */
    uname[strcspn(uname, "\r\n")] = '\0';
    strncpy(self->name, uname, sizeof self->name);

    /* announce join */
    char joinmsg[128];
    snprintf(joinmsg, sizeof joinmsg, "%s joined the chat\n", self->name);
    broadcast(clisockfd, joinmsg);
    printf("%s", joinmsg);

    /* ---------- chat loop ---------- */
    char buffer[256];
    int nrcv;

    while ((nrcv = recv(clisockfd, buffer, sizeof buffer - 1, 0)) > 0)
    {
        buffer[nrcv] = '\0';
        broadcast(clisockfd, buffer);
    }
    if (nrcv < 0)
        perror("recv");

    /* announce leave */
    char leavemsg[128];
    snprintf(leavemsg, sizeof leavemsg, "%s left the chat\n", self->name);
    broadcast(clisockfd, leavemsg);
    printf("%s", leavemsg);

    shutdown(clisockfd, SHUT_RDWR);
    close(clisockfd);

    /* ---------- remove from list ---------- */
    pthread_mutex_lock(&client_list_mutex);
    USR *prev = NULL, *cur = head;
    while (cur)
    {
        if (cur->clisockfd == clisockfd)
        {
            if (prev)
                prev->next = cur->next;
            else
                head = cur->next;
            if (cur == tail)
                tail = prev;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    print_client_list(); /* show new list while lock held   */
    pthread_mutex_unlock(&client_list_mutex);

    return NULL;
}

void print_client_list(void)
{
    printf("=== Connected clients ===\n");
    for (USR *cur = head; cur; cur = cur->next)
        printf("%s:%d  %s\n",
               inet_ntoa(cur->cli_addr.sin_addr),
               ntohs(cur->cli_addr.sin_port),
               *cur->name ? cur->name : "(no‑name yet)");
    if (!head)
        puts("(no clients connected)");
    puts("=========================");
}

static USR *find_user(int sock)
{
    for (USR *u = head; u; u = u->next)
        if (u->clisockfd == sock)
            return u;
    return NULL;
}