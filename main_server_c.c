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
#define CLR_RST "\033[0m"
static const char *CLR[] = {/* 7 safe ANSI colours           */
                            "\033[0;31m", "\033[0;32m", "\033[0;33m",
                            "\033[0;34m", "\033[0;35m", "\033[0;36m",
                            "\033[0;37m"};
#define NCOL (sizeof(CLR) / sizeof(CLR[0]))

static int next_colour = 0; /* rotates through CLR[]         */

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct _USR
{
    int clisockfd;
    struct sockaddr_in addr;
    char username[32];
    int colour; /* index into CLR[]              */
    int room;   /* chat‑room number              */
    struct _USR *next;
} USR;

static USR *head = NULL, *tail = NULL;
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
static void add_tail(USR *u)
{
    pthread_mutex_lock(&list_lock);
    if (!head)
        head = tail = u;
    else
    {
        tail->next = u;
        tail = u;
    }
    pthread_mutex_unlock(&list_lock);
}

static void remove_fd(int fd)
{
    pthread_mutex_lock(&list_lock);
    USR *cur = head, *prev = NULL;
    while (cur && cur->clisockfd != fd)
    {
        prev = cur;
        cur = cur->next;
    }
    if (cur)
    {
        if (prev)
            prev->next = cur->next;
        else
            head = cur->next;
        if (cur == tail)
            tail = prev;
        free(cur);
    }
    pthread_mutex_unlock(&list_lock);
}

static USR *find_user(int fd)
{
    pthread_mutex_lock(&list_lock);
    for (USR *c = head; c; c = c->next)
        if (c->clisockfd == fd)
        {
            pthread_mutex_unlock(&list_lock);
            return c;
        }
    pthread_mutex_unlock(&list_lock);
    return NULL;
}

static void print_addr_list(void)
{
    pthread_mutex_lock(&list_lock);
    printf("=== Connected clients ===\n");
    for (USR *c = head; c; c = c->next)
        printf("%s: %s\n", c->username[0]?c->username:"No Name",inet_ntoa(c->addr.sin_addr));
    printf("=========================\n");
    pthread_mutex_unlock(&list_lock);
}

static void broadcast_room(int room, int fromfd, const char *msg)
{
    pthread_mutex_lock(&list_lock);
    USR *sender = NULL;
    if (fromfd >= 0)
        for (USR *c = head; c; c = c->next)
            if (c->clisockfd == fromfd)
            {
                sender = c;
                break;
            }

    for (USR *c = head; c; c = c->next)
        if (c->room == room && c->clisockfd != fromfd)
        {
            char buf[600];
            int n;
            if (sender)
                n = snprintf(buf, sizeof(buf), "%s%s%s: %s", CLR[sender->colour], sender->username, CLR_RST, msg);
            else
                n = snprintf(buf, sizeof(buf), "%s", msg);
            send(c->clisockfd, buf, n, 0);
        }
    pthread_mutex_unlock(&list_lock);
}

/* ------------------------- thread routine -------------------------- */
static void *thread_main(void *arg)
{
    int clisockfd = *(int *)arg;
    free(arg);
    pthread_detach(pthread_self());

    char buf[256];
    int n;
    while ((n = recv(clisockfd, buf, 255, 0)) > 0)
    {
        buf[n] = '\0';
        USR *u = find_user(clisockfd);
        if (u)
            broadcast_room(u->room, clisockfd, buf);
    }

    /* client disconnects */
    USR *u = find_user(clisockfd); /* still valid until removed */
    if (u)
    {
        char notice[64];
        snprintf(notice, sizeof(notice), "%s has left the chat.\n", u->username);
        broadcast_room(u->room, -1, notice);
    }
    close(clisockfd);
    remove_fd(clisockfd);
    print_addr_list();
    return NULL;
}

/* ------------------------------ main ------------------------------ */
int main(void)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("opening socket");

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_NUM);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("binding");

    listen(sockfd, 5);

    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t clen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clen);
        if (newsockfd < 0)
            error("accept");
        printf("Connected: %s:%d\n", inet_ntoa(cli_addr.sin_addr),htons(PORT_NUM));

        /* ---------- username handshake ---------- */
        char uname[32] = {0};
        send(newsockfd, "Choose a unique username: ", 27, 0);
        if (recv(newsockfd, uname, 31, 0) <= 0)
        {
            close(newsockfd);
            continue;
        }
        uname[strcspn(uname, "\r\n")] = 0;

        /* check uniqueness */
        int dup = 0;
        pthread_mutex_lock(&list_lock);
        for (USR *c = head; c; c = c->next)
            if (!strcmp(c->username, uname))
            {
                dup = 1;
                break;
            }
        pthread_mutex_unlock(&list_lock);
        if (dup)
        {
            send(newsockfd, "Username taken. Disconnecting.\n", 32, 0);
            close(newsockfd);
            continue;
        }

        /* ---------- room handshake ---------- */
        char rbuf[16] = {0};
        send(newsockfd, "Select a Room # or 'new': ", 26, 0);
        if (recv(newsockfd, rbuf, 15, 0) <= 0)
        {
            close(newsockfd);
            continue;
        }
        int room = -1;
        int is_new_room = 0; //chatroom
        if (!strncmp(rbuf, "new", 3) || rbuf[0] == '\n')
        {
            pthread_mutex_lock(&list_lock);
            for (USR *c = head; c; c = c->next)
                if (c->room >= room)
                    room = c->room + 1;
            pthread_mutex_unlock(&list_lock);
            is_new_room = 1; //new chatroom created
        }
        else
            room = atoi(rbuf);
        if (room < 0)
            room = 0;

        /* ---------- build user node ---------- */
        USR *u = calloc(1, sizeof(USR));
        u->clisockfd = newsockfd;
        u->addr = cli_addr;
        strncpy(u->username, uname, 31);
        u->colour = next_colour++ % NCOL;
        u->room = room;
        add_tail(u);
        print_addr_list();

        if(is_new_room) // check for new chatroom
        {
            char notice[64];
            snprintf(notice, sizeof(notice), "Connected to %s with new room number %d\n", inet_ntoa(cli_addr.sin_addr), room);
            broadcast_room(room, -1, notice);
        }

        /* ---------- spawn thread ---------- */
        pthread_t tid;
        int *argfd = malloc(sizeof(int));
        *argfd = newsockfd;
        if (pthread_create(&tid, NULL, thread_main, argfd) != 0)
            error("pthread_create");
    }
    return 0;
}
