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
#include <signal.h>

#define PORT_NUM 5000

volatile int keep_running = 1;

void handle_sigint(int sig) {
	keep_running = 0;
}

void error(const char *msg) {
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void *thread_main_recv(void *args) {
	pthread_detach(pthread_self());
	int sockfd = ((ThreadArgs *)args)->clisockfd;
	free(args);

	char buffer[512];
	int n;

	while (keep_running && (n = recv(sockfd, buffer, 512, 0)) > 0) {
    	buffer[n] = '\0';
    	printf("%s\n", buffer);
	}

	if (n == 0) {
    	printf("Server closed the connection.\n");
    	exit(0);
	}
	if (n < 0)
    	error("ERROR recv() failed");

	return NULL;
}

void *thread_main_send(void *args) {
	pthread_detach(pthread_self());
	int sockfd = ((ThreadArgs *)args)->clisockfd;
	free(args);

	char buffer[256];
	int n;

	while (keep_running) {
    	memset(buffer, 0, 256);
    	if (fgets(buffer, 255, stdin) == NULL) {
        	shutdown(sockfd, SHUT_WR);
        	break;
    	}

    	buffer[strcspn(buffer, "\n")] = '\0';

    	if (strlen(buffer) == 0) {
        	printf("Empty input detected. Do you want to leave the chat? (y/n): ");
        	char choice;
        	scanf(" %c", &choice);
        	getchar(); // [AI] Consume leftover newline character from input buffer
        	if (choice == 'y' || choice == 'Y') {
            	printf("Leaving chat.\n");
            	shutdown(sockfd, SHUT_RDWR);
            	close(sockfd);
            	exit(0);
        	}
        	continue; // [AI] Stay in the chat if user doesn't confirm exit
    	}

    	n = send(sockfd, buffer, strlen(buffer), 0);
    	if (n < 0)
        	error("ERROR writing to socket");
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	signal(SIGINT, handle_sigint); // [AI] Gracefully handle Ctrl+C to shutdown cleanly

	if (argc < 3) {
    	printf("Usage: ./main_client_cp2 <server-ip> <room#|new>\n");
    	exit(1);
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
    	error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	if (connect(sockfd, (struct sockaddr *)&serv_addr, slen) < 0)
    	error("ERROR connecting");

	// [AI] Automatically process room selection from command-line argument
	if (strcmp(argv[2], "new") == 0) {
    	char msg[] = "ROOM_REQUEST_NEW";
    	send(sockfd, msg, strlen(msg), 0);
	} else {
    	char room_request[64];
    	snprintf(room_request, sizeof(room_request), "ROOM_REQUEST_JOIN:%s", argv[2]);
    	send(sockfd, room_request, strlen(room_request), 0);
	}

	// [AI] Read server response to confirm room connection
	char response[128];
	memset(response, 0, sizeof(response));
	recv(sockfd, response, sizeof(response) - 1, 0);
	printf("%s\n", response);

	// [AI] Exit if room request was invalid
	if (strstr(response, "Invalid") != NULL) {
    	printf("Failed to join room. Exiting.\n");
    	close(sockfd);
    	exit(1);
	}

	// [AI] Prompt user for username interactively
	char username[50];
	do {
    	printf("Type your user name: ");
    	fgets(username, sizeof(username), stdin);
    	username[strcspn(username, "\n")] = '\0'; // [AI] Remove trailing newline from username
	} while (strlen(username) == 0);

	send(sockfd, username, strlen(username), 0);

	printf("%s (%s) joined the chat room!\n", username, argv[1]);

	pthread_t tid1, tid2;
	ThreadArgs *args;

	args = malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void *)args);

	args = malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void *)args);

	pthread_join(tid1, NULL);

	close(sockfd);
	return 0;
}

