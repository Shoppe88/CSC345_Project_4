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
#define MAX_CLIENTS 100
#define NAME_LEN 50

#define NUM_COLORS (sizeof(colors) / sizeof(colors[0]))
#define COLOR_RESET "\033[0m"

const char *colors[] = {
    "\033[31m", // red
    "\033[32m", // green
    "\033[33m", // yellow
    "\033[34m", // blue
    "\033[35m", // magenta
    "\033[36m", // cyan
    "\033[91m", // light red
    "\033[92m", // light green
    "\033[93m", // light yellow
    "\033[94m", // light blue
    "\033[95m", // light magenta
    "\033[96m", // light cyan
};

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct _USR
{
    int clisockfd;
    struct sockaddr_in cli_addr; //added change
    char username[NAME_LEN]; // username
    char color[16];  // ANSI color code string, e.g., "\033[31m"
    struct _USR *next;
} USR;

USR *head = NULL;
USR *tail = NULL;

void send_join_message(USR *new_user); //print join message
void send_leave_message(USR *leaving_user);//print leave message
void print_client_list(); //recommended addition
void add_tail(int newclisockfd, struct sockaddr_in cliaddr, const char* username);
void broadcast(int fromfd, char *message);
void *thread_main(void *args);

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

        char username[NAME_LEN] = {0};
        int uname_len = recv(newsockfd, username, NAME_LEN - 1, 0);
        if (uname_len <= 0)
        {
            close(newsockfd);
            continue;
        }
        username[uname_len] = '\0';

        pthread_mutex_lock(&client_list_mutex);
        add_tail(newsockfd, cli_addr, username);
        pthread_mutex_unlock(&client_list_mutex);

        printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
        USR *new_user = tail;
        send_join_message(new_user);  // Send the join message to all clients

        // Print updated client list
        pthread_mutex_lock(&client_list_mutex);
        print_client_list();
        pthread_mutex_unlock(&client_list_mutex);

        // prepare USR structure to pass client socket
        USR *args = (USR *)malloc(sizeof(USR));
        if (args == NULL)
            error("ERROR creating thread argument");

        args->clisockfd = newsockfd;
        args->cli_addr = cli_addr; //recommended addtion
        strncpy(args->username, username, NAME_LEN);

        pthread_t tid;
        if (pthread_create(&tid, NULL, thread_main, (void *)args) != 0)
            error("ERROR creating a new thread");
    }

    return 0;
}

void add_tail(int newclisockfd, struct sockaddr_in cliaddr, const char* username)
{
    USR *new_node = (USR *)malloc(sizeof(USR));
    //error check recommended to handle edge cases
    if (!new_node)
        error("ERROR allocating memory for client node");
    
    //recommened adding new nodes when a new client 
    new_node->clisockfd = newclisockfd;
    new_node->cli_addr = cliaddr;
    strncpy(new_node->username, username, NAME_LEN);
    new_node->username[NAME_LEN - 1] = '\0';
    new_node->next = NULL;
    
    // Assign a color based on hash of username
    int hash = 0;
    for (int i = 0; username[i] != '\0'; ++i)
        hash += username[i];
    strncpy(new_node->color, colors[hash % NUM_COLORS], sizeof(new_node->color));

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
    // traverse through all connected clients
    pthread_mutex_lock(&client_list_mutex);
    USR *cur = head;
    while (cur != NULL)
    {
        // Don't send message back to the sender (only if fromfd is not -1)
        if (fromfd != -1 && cur->clisockfd != fromfd)
        {
            char buffer[512];
            sprintf(buffer, "%s", message);

            int nmsg = strlen(buffer);
            int nsen = send(cur->clisockfd, buffer, nmsg, 0);
            if (nsen != nmsg)
                perror("ERROR send() failed");
        }

        // send message to all clients
        if (fromfd == -1)
        {
            char buffer[512];
            sprintf(buffer, "%s", message);

            int nmsg = strlen(buffer);
            int nsen = send(cur->clisockfd, buffer, nmsg, 0);
            if (nsen != nmsg)
                perror("ERROR send() failed");
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&client_list_mutex);
}


void *thread_main(void *args)
{
    // make sure thread resources are deallocated upon return
    pthread_detach(pthread_self());

    // get socket descriptor from argument
    int clisockfd = ((USR *)args)->clisockfd;
    struct sockaddr_in cli_addr = ((USR *)args)->cli_addr;
    free(args);

    //-------------------------------
    // Now, we receive/send messages
    char buffer[256];
    int nrcv;

    while ((nrcv = recv(clisockfd, buffer, 256, 0)) > 0)
    {
        buffer[nrcv] = '\0';

        // Find sender in user list
        pthread_mutex_lock(&client_list_mutex);
        USR *cur = head;
        while (cur != NULL && cur->clisockfd != clisockfd)
            cur = cur->next;
        pthread_mutex_unlock(&client_list_mutex);

        if (cur != NULL)
        {
            char formatted[512];
            snprintf(formatted, sizeof(formatted), "%s%s: %s%s", cur->color, cur->username, buffer, COLOR_RESET);

            broadcast(clisockfd, formatted);
        }
    }

    
    if (nrcv < 0)
        error("ERROR recv() failed");

    char username[NAME_LEN];
    strncpy(username, ((USR *)args)->username, NAME_LEN);
    username[NAME_LEN - 1] = '\0'; 

    shutdown(clisockfd, SHUT_RDWR);
    close(clisockfd);

    // Send leave message before removing from list
    char leave_msg[256];
    snprintf(leave_msg, sizeof(leave_msg), "[Server]: %s (%s) has left the chat!",
            username, inet_ntoa(cli_addr.sin_addr));
    broadcast(-1, leave_msg);

    // Remove client from list
    pthread_mutex_lock(&client_list_mutex);
    USR *prev = NULL;
    USR *cur = head;

    while (cur != NULL)
    {
        if (cur->clisockfd == clisockfd)
        {
            if (prev == NULL)
            { // head removal
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

    pthread_mutex_lock(&client_list_mutex); //recommended additon
    print_client_list();
    pthread_mutex_unlock(&client_list_mutex);

    return NULL;
}

void print_client_list()
{
    printf("=== Connected clients ===\n");
    USR *cur = head;
    while (cur != NULL)
    {
        printf("%s:%d\n", inet_ntoa(cur->cli_addr.sin_addr), ntohs(cur->cli_addr.sin_port));
        cur = cur->next;
    }
    if (head == NULL)
    {
        printf("(no clients connected)\n");
    }
    printf("=========================\n");
}

void send_join_message(USR *new_user)
{
    // Format the join message
    char join_msg[256];
    snprintf(join_msg, sizeof(join_msg), "[Server]: %s (%s) has joined the chat!", new_user->username, inet_ntoa(new_user->cli_addr.sin_addr));

    // Debug: Print the join message to ensure it's correct
    printf("Join Message: %s\n", join_msg);

    // Broadcast the message to all clients
    broadcast(-1, join_msg);  // -1 indicates it's a system message (join)
}

void send_leave_message(USR *leaving_user)
{
    char leave_msg[256];
    snprintf(leave_msg, sizeof(leave_msg), "[Server]: %s (%s) has left the chat!",
             leaving_user->username, inet_ntoa(leaving_user->cli_addr.sin_addr));
    broadcast(-1, leave_msg);
}


