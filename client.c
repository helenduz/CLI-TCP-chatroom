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

#define NAME_SIZE 40
#define BUFFER_SIZE 2048

char name[NAME_SIZE];
struct sockaddr_in server_addr;
int sock;
// int exit_flag = 0;


void print_prompt(){
    printf("%s", ">>> ");
    fflush(stdout);
}


/* Thread that handles sending message from client to server */
void *handle_send(){
    char message[BUFFER_SIZE - NAME_SIZE - 16];
    char buffer_send[BUFFER_SIZE];
    while (1){
        fgets(message, BUFFER_SIZE - NAME_SIZE - 16, stdin);
        message[strcspn(message, "\n")] = '\0'; // trims space after newline char
        // exit if user entered "quit"
        if (strcmp(message, "quit") == 0){
            pthread_exit(0);
        }
        sprintf(buffer_send, ">>> %s: %s\n", name, message);
        if (send(sock, buffer_send, strlen(buffer_send), 0) <0){
            perror("Error occured when sending message to server");
            continue;
        }
        bzero(message, BUFFER_SIZE - NAME_SIZE - 16);
        bzero(buffer_send, BUFFER_SIZE);
    }
}


/* Thread that handles receiving messages from server */
void *handle_recv(){
    char buffer_recv[BUFFER_SIZE];
    while (1){
        int receive = recv(sock, buffer_recv, BUFFER_SIZE, 0);
        if (receive < 0){
            sleep(10);
            perror("Error occured when receiving message from server");
            continue;
        }
        else if (receive == 0){
            perror("Server side has initiated a shutdown, all messages received");
            // directly exit the program if server is closed
            printf("Quitting program now");
            sleep(5); // wait for send() to print out error to notify user 
            if (close(sock) < 0){
                perror("Error occured when closing socket");
            }
            exit(1);  
        }
        printf("%s", buffer_recv);
        bzero(buffer_recv, BUFFER_SIZE); 
    }
}


int main(int argc, char **argv){
    // gather and set server info
    char *server_ip = argv[1];
	int server_port = atoi(argv[2]);
    bzero(&server_addr, sizeof(server_addr)); // zero-init the struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    // create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("Error occured when creating socket");
    }

    // connect to server (i.e. setting peer address for future send()'s)
    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        perror("Error occured when connecting to server");
        return EXIT_FAILURE;
    }

    // check if max client is reached
    char init_buffer[BUFFER_SIZE];
    recv(sock, init_buffer, BUFFER_SIZE, 0);
    init_buffer[strcspn(init_buffer, "\n")] = '\0';
    printf("%s\n", init_buffer);
    if (init_buffer[0] == 'M'){
        printf("Quitting program now\n");
        if (close(sock) < 0){
        perror("Error occured when closing socket");
        }
        return EXIT_SUCCESS;
    }

    // get and send name to server
    printf("Enter your name: ");
    fgets(name, NAME_SIZE, stdin);
    name[strcspn(name, "\n")] = '\0'; // trim spaces after newline
    /* to do: impose size on name */
    if (send(sock, name, strlen(name), 0) <0){
        perror("Error occured when sending user name to server");
    }

    // create thread for handling receiving messages
    pthread_t recv_tid;
    if (pthread_create(&recv_tid, NULL, &handle_recv, NULL) != 0){
            perror("Error occured when creating receiving thread");
    }   

    // create thread for handling sending messages
    pthread_t send_tid;
    if (pthread_create(&send_tid, NULL, &handle_send, NULL) != 0){
            perror("Error occured when creating receiving thread");
    }  

    // // exit when exit_flag is set
    // while (1){
    //     if (exit_flag = 1){
    //         close(sock);
    //         return EXIT_SUCCESS;
    //     }
    // }
    void *status;
    if (pthread_join(send_tid, &status) != 0){
        perror("Error occured during pthread_join() for handle_send");
    }
    if (close(sock) < 0){
        perror("Error occured when closing socket");
    } 
    return EXIT_SUCCESS;
}