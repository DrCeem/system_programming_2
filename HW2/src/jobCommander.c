#include <stdio.h>
#include <stdlib.h>
#include <string.h>
# include <sys/types.h> 
# include <sys/socket.h> 
# include <netinet/in.h> 
# include <unistd.h>
# include <netdb.h>

int main(int argc, char* argv[]) {
    
     // If the arguments given are less than 4 print an error message and exit 
    if(argc < 4)  {
        fprintf(stderr,"Error : Usage Requires 4 arguments\n");
        exit(1);
    }

    // Initialize variables for the server name and the port number
    char* serverName = argv[1];
    int port_num = atoi(argv[2]);
    
    //Initialize and create a socket
    int sock;

    //Initialize a socket address struct 
    struct sockaddr_in server;
    struct sockaddr* server_ptr = (struct sockaddr*)&server;

    if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {

        fprintf(stderr, "Socket Creation Failed\n");
        exit(1);
    }

    //Set the proper socket options so the port can be re-used
    int options = 1;
    if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR , &options,sizeof(options))) < 0 ) {
        
        fprintf(stderr, "Setting Socket Options Failed\n");
        close(sock);
        exit(1);
    }

    // Initialize a hostent struct to get the host by name
    struct hostent* rem = gethostbyname(serverName);
    
    //Bind the socket to the address and port
    server.sin_family = AF_INET;
    server.sin_port = htons(port_num);
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length ) ;
    
    //Initiate a Connection with the server with connect()
    if(connect(sock, server_ptr ,sizeof(server)) < 0) {
        fprintf(stderr, "Failed to Initiate Connection\n");
        close(sock);
        exit(1);
    }

    //Find the command id based on the argumnets
    //if command is :
    //          issueJob -> Command ID = 1
    //          setConcurrency -> Command ID = 2
    //          stop -> Command ID = 3
    //          poll -> Command ID = 4
    //          exit -> Command ID = 5

    char command_id;

    if(strcmp(argv[3], "issueJob") == 0) 
        command_id ='1';
    else if(strcmp(argv[3], "setConcurrency") == 0)
        command_id ='2';
    else if(strcmp(argv[3],"stop") == 0)
        command_id ='3';
    else if(strcmp(argv[3],"poll") == 0)
        command_id = '4';
    else if(strcmp(argv[3],"exit") == 0)
        command_id = '5';
    else {
        fprintf(stderr, "Commander : Invalid Command, Exiting\n");
        close(sock);
        exit(1);
    }

    //Write the command id in the socket 
    if(write(sock,&command_id,1) != 1) {

        fprintf(stderr,"Failed to write command id\n");
        close(sock);
        exit(1);
    }

    //Write the rest of the arguments in the socket (if there are any)
    for(int i = 4; i < argc ; i++) { 

        // Write every argument of the arg vector (except the first 4) into the socket for the 
        // job Executor Server to read. The arguments are seperated by a white space 
        if (write(sock , argv[i] , strlen(argv[i])) < 0) {
            
            fprintf(stderr,"Failed to write\n");
            close(sock);
            exit(1) ;
        }
        //After each argument write a whitespace to seperate them and the '\0' character after the last argument
        if (i < argc - 1) {

            if (write(sock, " " , 1) < 0) {
                fprintf(stderr,"Failed to write\n");
                close(sock);
                exit(1) ;
            }
        }
        else {

            //Write the null character at the end so the executor server stops reading (if there is garbage) afterwards.
            if (write(sock, "\0" , 1) < 0) {
                fprintf(stderr,"Failed to write\n");
                close(sock);
                exit(1) ;
            }
        }
    }

    // Shut writting down in the socket to let the server know the full request has been sent
    shutdown(sock,SHUT_WR);

    // Read the response from the executor server

    //Initialize a buffer with 64 bytes and increase its size each time it fills up
    //Initialize an offset  to be able to read dynamically
    int buff_size = 64;
    char* reading_buffer = (char *)malloc(buff_size);
    int bytes_read;
    int offset = 0;

    // While there are characters in the socket read them and increase the size of the buffer whenever it fills up
    while((bytes_read = read(sock, reading_buffer + offset , buff_size - offset)) > 0)
    {
        offset += bytes_read;

        if(buff_size <= offset) {
            buff_size *= 2;
            reading_buffer = (char*)realloc(reading_buffer, buff_size); 
        }
    }
    //Shut down reading in the socket after response is sent
    shutdown(sock,SHUT_RD);

    //Print the response read from the executor-server 
    printf("%s\n", reading_buffer);

    //Close the socket and free the buffer
    close(sock);
    free(reading_buffer);

}