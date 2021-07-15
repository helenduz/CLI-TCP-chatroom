# CLI-TCP-chatroom
A simple command-line chatroom implemented with socket programming in C. Multiple clients can send messages to the group. 

## How to Run the Program 
1. Compile the files with `make`. If you need to compile the files separately, then use `make client` or `make server`.
2. On the machine where you choose to run the server, simply run the program with `./server`.
3. On the machine(s) where you choose to run the client(s), run the program with `./client <server_IP> <server_port>`, where `<server_IP>` should be the IPv4 address of the server in the [**dotted decimal notation**](https://en.wikipedia.org/wiki/Dot-decimal_notation) (for localhost, this would be 127.0.0.1). `<server_IP>` should be the server's port number as identified at the top of `server.c`.


## What's next
- [ ] Let the server program take in two arguments: the maximum client, and the port number to host this service. 
- [ ] Provide a list of online users when a new user logs in.
- [ ] Private (client-to-client) messaging. 
- [ ] Make the chat history available for download on both the client and server side. 
- [ ] Implement a chatroom administrator on the server side. 
