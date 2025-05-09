all:
	gcc -o chat_server chat_server.c -lpthread
	gcc -o chat_client chat_client.c
	gcc -o chat_server_full chat_server_full.c -lpthread
	gcc -o chat_client_full chat_client_full.c -lpthread
	gcc -o -pthread -Wall -O2 main_client_s.c -o main_client_s
	gcc -o main_server_scott main_server_scott.c -lpthread
	gcc -o main_client_t main_client_t.c -lpthread
	gcc -o main_client_c main_client_s.c -lpthread
	gcc -o main_server_c main_server_c.c -lpthread
	gcc -o client_c client_c.c -lpthread

clean:
	rm chat_server chat_client
	rm chat_server_full chat_client_full
	rm main_server_scott
	rm main_client_s
	rm main_client_c
	rm main_client_t
	rm main_server_c
	rm client_c

