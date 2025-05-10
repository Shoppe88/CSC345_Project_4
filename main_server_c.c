// SERVER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 5000
#define MAX_ROOMS 100   // [AI] Max allowed chat rooms
#define MAX_CLIENTS 100 // [AI] Max allowed clients

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct _USR
{
    int clisockfd;
    char name[50];
    char color[10];
    struct sockaddr_in addr;
    int room_id;
    struct _USR *next;
} USR;

typedef struct _Room
{
    int room_id;
    struct _Room *next;
} Room;

USR *head = NULL;
Room *room_list = NULL;
int next_room_id = 1;
int active_rooms = 0;   // [AI] Tracks current active rooms
int active_clients = 0; // [AI] Tracks current active clients

const char *colors[] = {
    "\033[1;31m", "\033[1;32m", "\033[1;33m",
    "\033[1;34m", "\033[1;35m", "\033[1;36m",
    "\033[0;91m", "\033[0;92m", "\033[0;93m",
    "\033[0;94m"};
int color_index = 0;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

Room *find_or_create_room(const char *request, int *is_new_room)
{
    if (strncmp(request, "ROOM_REQUEST_NEW", 16) == 0)
    {
        if (active_rooms >= MAX_ROOMS)
            return NULL; // [AI] Enforces room limit
        Room *new_room = malloc(sizeof(Room));
        new_room->room_id = next_room_id++;
        new_room->next = room_list;
        room_list = new_room;
        active_rooms++; // [AI] Increment room count
        *is_new_room = 1;
        return new_room;
    }
    else if (strncmp(request, "ROOM_REQUEST_JOIN:", 18) == 0)
    {
        int req_id = atoi(request + 18);
        Room *cur = room_list;
        while (cur)
        {
            if (cur->room_id == req_id){
                *is_new_room = 0;
                return cur;
            }
            cur = cur->next;
        }
        return NULL;
    }
    return NULL;
}

void print_client_list()
{
    printf("=== Connected Clients ===\n");
    USR *cur = head;
    while (cur)
    {
        printf("%s:%d (Room %d)\n", inet_ntoa(cur->addr.sin_addr), ntohs(cur->addr.sin_port), cur->room_id);
        cur = cur->next;
    }
    if (!head)
        printf("(no clients connected)\n");
    printf("=========================\n");
}

void add_client(int clisockfd, struct sockaddr_in cli_addr, int room_id)
{
    if (active_clients >= MAX_CLIENTS)
    { // [AI] Enforces max clients limit
        char msg[] = "Server is full. Connection refused.\n";
        send(clisockfd, msg, strlen(msg), 0);
        close(clisockfd);
        return;
    }

    USR *new_node = malloc(sizeof(USR));
    new_node->clisockfd = clisockfd;
    new_node->addr = cli_addr;
    new_node->room_id = room_id;
    snprintf(new_node->color, sizeof(new_node->color), "%s", colors[color_index++ % 10]);
    new_node->next = head;
    head = new_node;
    active_clients++; // [AI] Increment active clients
}

void broadcast(int fromfd, int room_id, char *message)
{
    pthread_mutex_lock(&client_list_mutex);
    USR *sender = head;
    while (sender && sender->clisockfd != fromfd)
        sender = sender->next;

    USR *cur = head;
    while (cur)
    {
        if (cur->clisockfd != fromfd && cur->room_id == room_id)
        {
            char buffer[512];
            if (sender && strlen(sender->name) > 0)
                snprintf(buffer, sizeof(buffer), "%s[%s] %s\033[0m", sender->color, sender->name, message);
            else
                snprintf(buffer, sizeof(buffer), "[Unknown]: %s", message);

            send(cur->clisockfd, buffer, strlen(buffer), 0);
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&client_list_mutex);
}

typedef struct _ThreadArgs
{
    int clisockfd;
    struct sockaddr_in cli_addr;
} ThreadArgs;

void *thread_main(void *args)
{
    pthread_detach(pthread_self());
    int clisockfd = ((ThreadArgs *)args)->clisockfd;
    struct sockaddr_in cli_addr = ((ThreadArgs *)args)->cli_addr;
    free(args);

    char buffer[256];
    int nrcv;

    int is_new_room = 0;
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        nrcv = recv(clisockfd, buffer, sizeof(buffer) - 1, 0);
        if (nrcv <= 0)
        {
            close(clisockfd);
            return NULL;
        }

        // [AI] Handle Room List Request for Checkpoint 3
        if (strncmp(buffer, "ROOM_REQUEST_LIST", 17) == 0)
        {
            char list_response[1024] = "Server says following options are available:\n";
            Room *cur = room_list;
            if (!cur)
            {
                strcat(list_response, "No rooms available. You will be assigned a new room.\n");
            }
            else
            {
                while (cur)
                {
                    int user_count = 0;
                    USR *usr = head;
                    while (usr)
                    {
                        if (usr->room_id == cur->room_id)
                            user_count++;
                        usr = usr->next;
                    }
                    char room_info[100];
                    snprintf(room_info, sizeof(room_info), "Room %d: %d people\n", cur->room_id, user_count);
                    strcat(list_response, room_info);
                    cur = cur->next;
                }
            }
            // strcat(list_response, "Choose the room number or type [new] to create a new room:\n");
            send(clisockfd, list_response, strlen(list_response), 0);
            continue; // [AI] Continue waiting for user selection
        }

        Room *room = find_or_create_room(buffer, &is_new_room);
        if (!room)
        {
            char deny_msg[] = "Invalid room number or server full. Try again.\n";
            send(clisockfd, deny_msg, strlen(deny_msg), 0);
            continue;
        }

        char room_msg[128];
        snprintf(room_msg, sizeof(room_msg), "Connected to room %d.\n", room->room_id);
        send(clisockfd, room_msg, strlen(room_msg), 0);

        pthread_mutex_lock(&client_list_mutex);
        add_client(clisockfd, cli_addr, room->room_id);
        print_client_list();
        pthread_mutex_unlock(&client_list_mutex);
        break;
    }

    USR *self = head;
    while (self && self->clisockfd != clisockfd)
        self = self->next;

    if (!self)
    {
        close(clisockfd);
        return NULL;
    }

    memset(self->name, 0, sizeof(self->name));
    recv(clisockfd, self->name, sizeof(self->name) - 1, 0);

    if (is_new_room) {
        char room_announcement[256];
        snprintf(room_announcement, sizeof(room_announcement),
                "%s%s created the chat room %d!\n\033[0m", self->color, self->name, self->room_id);
        printf("%s", room_announcement);
        broadcast(clisockfd, self->room_id, room_announcement);
        send(clisockfd, room_announcement, strlen(room_announcement), 0);
    }

    char joinmsg[256];
    snprintf(joinmsg, sizeof(joinmsg), "%s%s (%s) joined the chat room!\n\033[0m", self->color, self->name, inet_ntoa(self->addr.sin_addr));
    printf("%s", joinmsg);
    broadcast(clisockfd, self->room_id, joinmsg);

    while ((nrcv = recv(clisockfd, buffer, 255, 0)) > 0)
    {
        buffer[nrcv] = '\0';
        broadcast(clisockfd, self->room_id, buffer);
    }

    char leavemsg[256];
    snprintf(leavemsg, sizeof(leavemsg), "%s%s (%s) left the room!\n\033[0m", self->color, self->name, inet_ntoa(self->addr.sin_addr));
    printf("%s", leavemsg);
    broadcast(clisockfd, self->room_id, leavemsg);
    close(clisockfd);

    pthread_mutex_lock(&client_list_mutex);
    USR *prev = NULL, *cur = head;
    while (cur)
    {
        if (cur->clisockfd == clisockfd)
        {
            if (!prev)
                head = cur->next;
            else
                prev->next = cur->next;
            free(cur);
            active_clients--; // [AI] Decrement client count
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    print_client_list();
    pthread_mutex_unlock(&client_list_mutex);

    return NULL;
}

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // [AI] Allows quick port reuse

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_NUM);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);

    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t clen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clen);
        if (newsockfd < 0)
            error("ERROR on accept");

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->clisockfd = newsockfd;
        args->cli_addr = cli_addr;

        pthread_t tid;
        pthread_create(&tid, NULL, thread_main, (void *)args);
    }
}
