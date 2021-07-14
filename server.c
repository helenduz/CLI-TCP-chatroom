#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 3
#define NAME_SIZE 40
#define BUFFER_SIZE 2048
#define SERVER_PORT 6666

/* structure to store client info */
typedef struct{
	struct sockaddr_in addr;
	int client_socket; // file descriptor, from accept()
	int client_id;
	char client_name[NAME_SIZE];
} client_t;
client_t *client_list[MAX_CLIENTS]; 
struct sockaddr_in server_addr; // socketaddr_in stores information about an IP socket
int server_socket; // file descriptor, from socket()
static volatile int client_count;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; // separate dequeque & enqueue & ensure_max


/* returns a file descriptor for a listening socket to be used by server */
int get_listen_socket(){
    // create server socket file descriptor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0){
        perror("Error occured when creating server socket\n");
        return EXIT_FAILURE;
    }

    // fill in info about server socket address
    bzero(&server_addr, sizeof(server_addr)); // zero-init the struct
    server_addr.sin_family = AF_INET;                
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY means 0.0.0.0 (all available interfaces)
    server_addr.sin_port = htons(SERVER_PORT);

    // set socket option to allow reuse of local address
    // i.e. allow the port to be used again immediately instead of waiting for timeouts on the TCP stack
    int option = 1;  
    if((setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)))<0){  
        perror("Error occured in setsockopt()\n");  
        return EXIT_FAILURE; 
    }

    // bind file descriptor to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Error occured when binding server socket\n");
        return EXIT_FAILURE;
    }

    // set socket to listen (able to accept connections)
    if (listen(server_socket, MAX_CLIENTS) < 0){
        perror("Error occured when setting server socket to listen\n");
        return EXIT_FAILURE;
    }

    printf("LISTENING PORT ESTABLISHED: %d\n", SERVER_PORT);
    return server_socket;    
}


/* adds a new (pointer to) client struct into the client_list 
*  due to the multi-thread approach, no guarantee that clients will be added in order
*/
void enqueue(client_t *new_client_struct){
    pthread_mutex_lock(&client_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++){
		if (!client_list[i]){ // note: global variables automatically initialized to zero
			client_list[i] = new_client_struct;
			break;
		}
	}
    client_count++;
    pthread_mutex_unlock(&client_mutex);
}


/* adds a new (pointer to) client struct into the client_list */
void dequeque(int client_id){
	pthread_mutex_lock(&client_mutex);
	for(int i = 0; i < MAX_CLIENTS; i++){
		if (client_list[i]){
			if (client_list[i]->client_id == client_id){
				client_list[i] = NULL;
				break;
			}
		}
	}
    client_count--;
	pthread_mutex_unlock(&client_mutex);
}


/* send content in buffer to all client sockets except for the sender itself */
void send_message(char *buffer, int sender_client_id){
	pthread_mutex_lock(&client_mutex);
	for(int i = 0; i < MAX_CLIENTS; i++){
		if (client_list[i] && (client_list[i]->client_id != sender_client_id)){
            if (send(client_list[i]->client_socket, buffer, strlen(buffer), 0) < 0){
                perror("Error occured when sending message to client\n");
                break;
            }
		}
	}
	pthread_mutex_unlock(&client_mutex);   
}


/* thread responsible for handling communication between server and a client 
*  (from entering the name to client quiting the connection)
*/
void *handle_one_client(void *arg){
    client_t *cur_client = (client_t *) arg;
    char buffer[BUFFER_SIZE];
    char name[NAME_SIZE];
    int exit_flag = 0;

    // capture the client's name
	if(recv(cur_client->client_socket, name, NAME_SIZE, 0) <= 0){
		exit_flag = 1;
	}
    else{
		strcpy(cur_client->client_name, name);
		sprintf(buffer, "--- %s has joined ---\n", cur_client->client_name);
		printf("%s", buffer);
		send_message(buffer, cur_client->client_id);
	}

    bzero(buffer, BUFFER_SIZE);

    // wait for message to arrive from client
    while(1){
        if (exit_flag){
            break;
        }
        int receive = recv(cur_client->client_socket, buffer, BUFFER_SIZE, 0);
        // when client has ended connection or typed exit
        if (receive == 0 || strcmp(buffer, "quit") == 0){
            sprintf(buffer, "--- %s has left ---\n", cur_client->client_name);
            printf("%s", buffer);
            send_message(buffer, cur_client->client_id);
            exit_flag = 1;
        }
        else if (receive > 0){
            send_message(buffer, cur_client->client_id);
            printf("%s", buffer);
        }
        // if receive = -1 then error has occured
		else{
			printf("Error occured when receiving client message\n");
			exit_flag = 1;
		}
		bzero(buffer, BUFFER_SIZE); // empty buffer after every message
    }

    close(cur_client->client_socket);
    dequeque(cur_client->client_id);
    free(cur_client);
    pthread_exit(0);
}


int main(int argc, char **argv){
    client_count = 0;
    pthread_t tid; // thread id (overwritten for every new thread created)
    struct sockaddr_in client_addr; // overwritten for every new client connected
    int client_socket; // file descriptor, from accept() 
    int client_id = 0; // will be assigned as client_id's (note: != index to client_list)

    server_socket = get_listen_socket();

    while (1){
        // accept() will wait for client connection (create new socket + allocates new file descriptor)
        int addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, (socklen_t *) &addr_len);
        if (client_socket < 0){
            perror("Error occured when accepting client\n");
            continue;
        }

        // ensure max client is not exceeded
        pthread_mutex_lock(&client_mutex);
        if (client_count == MAX_CLIENTS){
            char max_error[32];
            sprintf(max_error, "Max clients (%d) reached.\n", MAX_CLIENTS);
            send(client_socket, max_error, strlen(max_error), 0);
            printf("%s", max_error);
            printf("Rejected client: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            sleep(5);
            if (close(client_socket) < 0){
                perror("Error occured when closing socket for rejected client");
            }
            pthread_mutex_unlock(&client_mutex);
            continue;       
        }
        else{
            char conn_success[96];
            sprintf(conn_success, "-------Chat Room Connection Successful. Type quit to exit--------\n");
            send(client_socket, conn_success, strlen(conn_success), 0);
        }
        pthread_mutex_unlock(&client_mutex);

        printf("New client connection: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // fill in client struct for the new client
		client_t *new_client_struct = (client_t *) malloc(sizeof(client_t));
		new_client_struct->addr = client_addr;
		new_client_struct->client_socket = client_socket;
		new_client_struct->client_id = client_id++;

		/* Add client to the queue and create thread */
		enqueue(new_client_struct);
		if (pthread_create(&tid, NULL, &handle_one_client, (void *) new_client_struct) != 0){
            perror("Error occured when creating client thread\n");
        }   
    }   
    return EXIT_SUCCESS;
}