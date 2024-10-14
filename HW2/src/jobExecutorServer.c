#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include "queue.h"
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BACKLOG 20

/* Define a structure that stores the information that needs to be passed to the controller threads
That information is : 1) The size of the buffer containing the waiting jobs
                      2) The file descriptor of the socket that is created to communicate with each specific client
                      3) The file descriptor of the initial socket that accepts connections on the server */

typedef struct thread_arguments {

    int buffer_size;
    int clientSocket;
    int initialSocket;

} Thread_Arguments;

// Define A function to be called when a controller thread is running
void* controller(void* args);

// Define A function to be called when a worker thread is running
void* worker();


//Initialize a global variable for the concurrency and declare a mutex for it
int concurrency = 1;
pthread_mutex_t concurrency_mutex = PTHREAD_MUTEX_INITIALIZER;

//Initialize a global variable for the number of running jobs and declare a mutex for it
int running = 0;
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;

//Initialize a global variable to assign new (unique) job ID to the new jobs and declare a mutex for it
int jobID = 0;
pthread_mutex_t jobID_mutex = PTHREAD_MUTEX_INITIALIZER;

//Declare a global queue (pointer) as the waiting buffer as well as a mutex for it
Queue* buffer;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

//Declare a condition variable and a mutex for it for letting the worker threads know whenever an new job is issued (the buffer becomes non-empty) and concurrency allows it
pthread_cond_t condition_var_worker = PTHREAD_COND_INITIALIZER;
pthread_mutex_t condition_var_worker_mutex = PTHREAD_MUTEX_INITIALIZER;

//Declare a boolean variable to tell if the buffer is empty or not as well as a mutex for it
bool empty = true;
pthread_mutex_t empty_mutex = PTHREAD_MUTEX_INITIALIZER;

//Declare a condition variable and a mutex for it for letting the controller threads know whenever the buffer fills up 
pthread_cond_t condition_var_controller = PTHREAD_COND_INITIALIZER;
pthread_mutex_t condition_var_controller_mutex = PTHREAD_MUTEX_INITIALIZER;

//Declare a boolean variable to tell if the buffer is full or not as well as a mutex for it
bool full = false;
pthread_mutex_t full_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize a global variable to let the threads know if they should exit and declare a mutex for it 
bool exit_received = false;
pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;


//////////////////////////////////////////////////////// MAIN THREAD SECTION ///////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[]) {

    // If the arguments given are not 4 print an error message and exit 
    if(argc != 4)  {
        printf("Error : Usage Requires 4 arguments\n");
        exit(1);
    }

    // Initialize variables for the port number and the buffer size
    int port_num = atoi(argv[1]);
    int buffer_size = atoi(argv[2]);

    
    //Create an empty queue for the initialization of the buffer
    buffer = queue_create();

    //Create an inital socket to accept connections from clients
    int initialSocket, clientSocket;
    //Initialize a socket address struct to bind to the port
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if((initialSocket = socket(AF_INET,SOCK_STREAM,0)) == -1) {

        fprintf(stderr, "Socket Creation Failed\n");
        exit(1);
    }

    //Set the proper socket options
    int options = 1;
    if ((setsockopt(initialSocket, SOL_SOCKET, SO_REUSEADDR, &options,sizeof(options))) < 0 ) {
        
        fprintf(stderr, "Setting Socket Options Failed\n");
        exit(1);
    }
    
    // Bind the socket to the address (INADDR_ANY) and port
    address.sin_family = AF_INET;
    address.sin_port = htons(port_num);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(initialSocket, (struct sockaddr *)&address , sizeof(address) ) < 0 ) {
        fprintf(stderr, "Socket Binding Failed\n");
        exit(1);
    }

    //Listen for connections in the socket 
    if(listen(initialSocket, BACKLOG) < 0) {
        fprintf(stderr, "Listen Failed\n");
        exit(1);
    }

    // Get the number of initial worker theads from the arguments
    int worker_thread_num = atoi(argv[3]);
    
    //Initialize an array to store the worker threads
    pthread_t worker_threads[worker_thread_num];

    int error;

    //Create all the worker threads and if any fail print an error message and exit
    for(int i = 0; i < worker_thread_num; i++) {

        if (error = pthread_create(&worker_threads[i] , NULL, worker , NULL ) ) {

            fprintf(stderr, "pthread_create : %s\n" , strerror(error) );
            exit(1);
        }

    }

    // Dynamically allocate an "array" to store the controller threads that are created
    pthread_t* controller_thread = (pthread_t*)malloc(sizeof(pthread_t)); 
    //Initialize a variable to keep track of the number of controller threads that have been created
    int controller_thread_num = 0;

    //Wait for connections in the socket and whenever one comes create a new socket and a controller thread to handle the communication with the client 
    //Stop whenever exit command has been received (exit_received variable is true)
    while(!exit_received) {
        
        
        //Allocate memeory for the controller thread arguments
        Thread_Arguments* thread_args = malloc(2*sizeof(int));

        // Accept incoming connections and for each one create a controller thread to handle the connection.
        //The socket file descriptor that will be used to communicate with each client is stored in clientSocket and is passed as an argument in the controller thread

        if((clientSocket = accept(initialSocket,(struct sockaddr *)&address, (socklen_t*)&addrlen )) < 0 ) {

            //If it fails it means the jobExecutor server is no longer able to read and write in the socket cause "exit" has been received
            continue;
        }

        //Get the buffer size and descriptor of the initial and new client socket in the controller thread arguments
        thread_args->clientSocket = clientSocket;
        thread_args->buffer_size = buffer_size;
        thread_args->initialSocket = initialSocket;

        //Create the controller thread and if it fails print an error message and exit
        if (error = pthread_create(&controller_thread[controller_thread_num] , NULL, controller , (void*)thread_args ) ) {

            fprintf(stderr, "pthread_create : %s\n" , strerror(error) );
            exit(1);
        }
        //Increase the number of controller threads by 1 
        controller_thread_num++;
        
        //Resize the array storing the controller threads since it changes dynamically
        controller_thread = (pthread_t*)realloc(controller_thread, (controller_thread_num + 1) * sizeof(pthread_t) );

    }

    //Once an exit command has been sent by a commander the loop will break
    //Wait for all the controller threads to complete execution with pthread_join
    for(int i = 0; i < controller_thread_num; i++) {

        // Wait for the controller thread to finish execution
        if ( error = pthread_join ( controller_thread[i] , NULL) ) { 
                
                fprintf(stderr, "pthread_join : %s\n" , strerror(error) );
                exit(1);
        }
    }

    // Wait for each worker thread to finish execution
    for(int j = 0; j < worker_thread_num; j++) {

        if (error = pthread_join(worker_threads[j] , NULL ) ){

            fprintf(stderr, "pthread_join : %s\n" , strerror(error) );
            exit(1);
        }

    }

    //Close the initial socket
    close(initialSocket);

    //Destroy the buffer
    queue_destroy(buffer);

    //Destroy all the mutexes and the condition variable
    pthread_mutex_destroy(&concurrency_mutex);
    pthread_mutex_destroy(&running_mutex);
    pthread_mutex_destroy(&jobID_mutex);
    pthread_mutex_destroy(&buffer_mutex);
    pthread_mutex_destroy(&condition_var_worker_mutex);
    pthread_mutex_destroy(&empty_mutex);
    pthread_mutex_destroy(&full_mutex);
    pthread_mutex_destroy(&exit_mutex);
    pthread_cond_destroy(&condition_var_worker);

    pthread_exit(NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////// CONTROLLER THREAD ////////////////////////////////////////////////////////////////////////
void* controller(void* args) {
    
    // Get the thread argument struct for the thread to access the shared information  
    Thread_Arguments* arguments = (Thread_Arguments*)args ;

    //Initialize a variable and read the command id from the socket
    char command_id;

    if(read(arguments->clientSocket,&command_id,1) != 1 ) {
        fprintf(stderr, "Controller : Read Failed\n");
        exit(1);
    }

    //If command id == 1, the command is issueJob
    if(command_id == '1'){

        //Initialize a buffer dynamically to read the job that was issued 
        int buff_size = 64;
        char* read_buffer = (char *)malloc(buff_size);
        int bytes_read;
        int offset = 0;

        // While there are characters in the socket read them and increase the size of the buffer whenever it fills up
        while((bytes_read = read(arguments->clientSocket, read_buffer + offset , buff_size - offset)) > 0)
        {

            offset += bytes_read;

            if(buff_size <= offset) {
                buff_size *= 2;
                read_buffer = (char*)realloc(read_buffer, buff_size); 
            }
        }
        
        //Shut reading down after reading the client request
        shutdown(arguments->clientSocket,SHUT_RD);

        //If the buffer is full the controller thread sleeps until there is space in the buffer for it to issue the job (with the use of condition variables)
        // Wait on the condition variable using the mutex, a while loop and pthread_cond_wait
        pthread_mutex_lock(&condition_var_controller_mutex);

        pthread_mutex_lock(&full_mutex);

        while(full) {
            pthread_mutex_unlock(&full_mutex);

            pthread_cond_wait(&condition_var_controller, &condition_var_controller_mutex);

            pthread_mutex_lock(&full_mutex);
        }

        pthread_mutex_unlock(&full_mutex);

        pthread_mutex_unlock(&condition_var_controller_mutex);



        if(exit_received)
            pthread_exit(NULL);

        // Lock the buffer mutex so other threads don't have access to it while the critcal section is running
        pthread_mutex_lock(&buffer_mutex);

        //Lock the mutex for the jobID so there is no race condition when assigning new ids to jobs (if there are multiple controller threads running).
        pthread_mutex_lock(&jobID_mutex);

        //Create the new jobID (as a string)
        char new_jobID[15] = "job_";
        snprintf(new_jobID + 4, sizeof(new_jobID) , "%d" , jobID );
        jobID++;

        //Unlock the mutex so other controllers cna assign new (unique) jobIDs to new jobs coming
        pthread_mutex_unlock(&jobID_mutex);

        // Create the new triplet for the new job
        Triplet* triplet = triplet_create(new_jobID,read_buffer,arguments->clientSocket);

        //Insert the triplet for the job in the buffer 
        queue_insert_rear(buffer, triplet);

        //Lock the mutex for the "full buffer" condition variable so other controllers will wait if the buffer was just filled 
        pthread_mutex_lock(&condition_var_controller_mutex);

        if(buffer->size == arguments->buffer_size)  {
            pthread_mutex_lock(&full_mutex);
            full = true;
            pthread_mutex_unlock(&full_mutex);
        }
        
        pthread_mutex_unlock(&condition_var_controller_mutex);

        pthread_mutex_unlock(&buffer_mutex);

        // Lock the mutex for the worker condition variable so the controller waits until a worker is ready (if needed)
        // in order to awake it once the new job has been inserted in the buffer
        pthread_mutex_lock(&condition_var_worker_mutex);
        
        //If the empty variable was true change it to false so a worker thread is awakened
        pthread_mutex_lock(&empty_mutex);
        empty = false;
        pthread_mutex_unlock(&empty_mutex);
        
        pthread_cond_signal(&condition_var_worker);

        pthread_mutex_unlock(&condition_var_worker_mutex);

        //Send the response to the job commander through the client socket
        if(write(arguments->clientSocket,"JOB <",5) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }
        if(write(arguments->clientSocket,triplet->jobID,strlen(triplet->jobID)) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }
        if(write(arguments->clientSocket," , ",3) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
            
        } 
        if(write(arguments->clientSocket, triplet->job, strlen(triplet->job)) < 0) {
            
            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }
        if(write(arguments->clientSocket, "> SUBMITTED\n", strlen("> SUBMITTED\n")) < 0) {
            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }

        free(read_buffer);

    }
    //If command id == 2, then the command is set concurrency
    else if (command_id == '2') {
        
        //Initialize a buffer to read the new concurrency
        //Initialize an offset variable so the pipe contents can be read dynamically
        char* read_buffer = calloc(5,1);
        int offset=0;

        while(read(arguments->clientSocket, read_buffer+offset, 1) > 0) {
            offset++;
        }

        //Shut reading down after reading the client request
        shutdown(arguments->clientSocket,SHUT_RD);
        
        //Convert the new concurrency to an integer 
        int new_concurrency = atoi(read_buffer);

        //Lock the mutex for the concurrency before changing it so other threads can't access it while the critical section is running
        pthread_mutex_lock(&concurrency_mutex);

        concurrency = new_concurrency;

        pthread_mutex_lock(&running_mutex);
        
        // Send a signal to awaken worker threads if the new concurrency allows it 
        for(int i = running ; i < concurrency; i++) {

            pthread_cond_signal(&condition_var_worker);

        }
        
        pthread_mutex_unlock(&running_mutex);

        pthread_mutex_unlock(&concurrency_mutex);

        //Send the response to the job commander through the client socket
        char response[35] = "CONCURRENCY SET AT ";
        strcat(response,read_buffer);
        
        if(write(arguments->clientSocket,response,strlen(response)) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }

        //Shut writing down in the socket after the response is sent
        shutdown(arguments->clientSocket,SHUT_WR);

        // Close the client socket after the response is sent and free the buffer used for reading 
        close(arguments->clientSocket);
        free(read_buffer);
    }
    //If command id == 3, then the command is stop
    else if (command_id == '3') {
        
        //Initialize a buffer to read the jobID that should be removed
        //Initialize an offset variable so the pipe contents can be read dynamically
        char* read_buffer = calloc(5,1);
        int offset=0;

        while(read(arguments->clientSocket, read_buffer+offset, 1) > 0) {
            offset++;
        }

        //Shut reading down after reading the client request
        shutdown(arguments->clientSocket,SHUT_RD);

        // Lock the mutex for the buffer so other threads can't access it while the critical section is running
        pthread_mutex_lock(&buffer_mutex);

        //Remove the job with the given id from the buffer (if it exists)
        Triplet* removed = queue_remove_jobID(buffer,read_buffer);

        if(buffer->size == 0) {

            pthread_mutex_lock(&empty_mutex);
            empty = true;
            pthread_mutex_unlock(&empty_mutex);
        }

        // Lock the  mutex for the "full buffer" condition variable which will signal (wake up) a controller waiting to issue a new job 
        pthread_mutex_lock(&condition_var_controller_mutex);

        if(buffer->size < arguments->buffer_size) {

            pthread_mutex_lock(&full_mutex);
            full = false;
            pthread_mutex_unlock(&full_mutex);

            pthread_cond_signal(&condition_var_controller);
        }

        pthread_mutex_unlock(&condition_var_controller_mutex);


        pthread_mutex_unlock(&buffer_mutex);

        //If removed is NULL it means the job id wasn't found in the buffer
        if(removed == NULL) {

            //Create an error message 
            char not_found[35] = "JOB <";
            strcat(not_found,read_buffer);
            strcat(not_found,"> NOTFOUND");

            //Write the error message in the client socket 
            if(write(arguments->clientSocket, not_found,strlen(not_found)) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }
        }
        else {

            //Create a response 
            char response[35] = "JOB <";
            strcat(response,read_buffer);
            strcat(response,"> REMOVED");

             //Write the response message in the client socket
            
            if(write(arguments->clientSocket, response ,strlen(response)) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }

            //Write the same response to the client that submitted the job
            if(write(removed->clientSocket, response ,strlen(response)) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }

            //Close the socket of the client that issued the job
            close(removed->clientSocket);
            //Destroy the pair that was removed
            triplet_destroy(removed);

        }

        //Shut writing down in the socket after the response is sent
        shutdown(arguments->clientSocket,SHUT_WR);

        // Close the client socket after the response is sent and free the buffer used for reading 
        close(arguments->clientSocket);
        free(read_buffer);
    }
    //If the command id == 4, then the command is poll
    else if (command_id == '4'){

        //Shut reading down after reading the client request
        shutdown(arguments->clientSocket,SHUT_RD);
        
        //Lock the buffer mutex so other threads can't access the queue while the critical section is running
        pthread_mutex_lock(&buffer_mutex);

        //Write a message to indicate that this is the "polled" list
        if(write(arguments->clientSocket, "\nJOBS IN BUFFER:\n\n",strlen("\nJOBS IN BUFFER:\n\n")) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }

        //For each job pair in the buffer send it as a string to the commander through the client socket
        for(Node* node = buffer->front; node != NULL; node = node->next) {

            if(write(arguments->clientSocket, "<",1) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }

            if(write(arguments->clientSocket, node->triplet->jobID,strlen(node->triplet->jobID)) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }

            if(write(arguments->clientSocket, " , ",3) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }
            if(write(arguments->clientSocket, node->triplet->job,strlen(node->triplet->job)) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }

            if(write(arguments->clientSocket, ">\n", 2) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(arguments->clientSocket);
                exit(1);
            }

        }
        // Send the Null character at the end of the response
        if(write(arguments->clientSocket, "\0", 1) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            close(arguments->clientSocket);
            exit(1);
        }
        //Unlock the mutex so other resources can access it
        pthread_mutex_unlock(&buffer_mutex);

        //Shut writing down in the socket after the response is sent
        shutdown(arguments->clientSocket,SHUT_WR);

        close(arguments->clientSocket);
    }
    //If the command id == 5, then the command is exit
    else if (command_id == '5') {

        //Shut reading down after reading the client request
        shutdown(arguments->clientSocket,SHUT_RD);

        //Lock the exit mutex so no other controller thread can access it
        pthread_mutex_lock(&exit_mutex);

        if (!exit_received)
            exit_received = true;

        pthread_mutex_unlock(&exit_mutex);

        //Send all the clients that have issued a job a message that the server has been terminated before the execution of the job
        for(Node* node = buffer->front; node != NULL; node = node->next) {

            if(write(node->triplet->clientSocket,"SERVER TERMINATED BEFORE EXECUTION\n",strlen("SERVER TERMINATED BEFORE EXECUTION\n")) < 0) {

                fprintf(stderr,"Server: Write Failed\n");
                close(node->triplet->clientSocket);
                exit(1);
            }
            // Close the client socket for each commander that had issued a job and the job didn't execute
            close(node->triplet->clientSocket);

        }

        //Send the commander that sent the exit command a response to let him know the server has been terminated
        if(write(arguments->clientSocket,"SERVER TERMINATED\n",strlen("SERVER TERMINATED\n")) < 0) {

            fprintf(stderr,"Server: Write Failed\n");
            exit(1);
        }

        //Close the client socket (the one that sent the exit command) 
        close(arguments->clientSocket);

        //Shut dowm the initial communication socket so the main thread unblocks on the accept() call
        shutdown(arguments->initialSocket,SHUT_RD);

        // Lock the mutex for the condition variable so the controller waits until a worker is ready (if needed)
        // in order to awake it so it completes exectuion and exits
        pthread_mutex_lock(&condition_var_worker_mutex);
        
        //If the empty variable was true change it to false so a worker thread is awakened
        pthread_mutex_lock(&empty_mutex);
        empty = false;
        pthread_mutex_unlock(&empty_mutex);

        pthread_mutex_lock(&running_mutex);
        running = 0;
        pthread_mutex_unlock(&running_mutex);
        //Wake up all the worker theads so they complete execution
        pthread_cond_broadcast(&condition_var_worker);

        pthread_mutex_unlock(&condition_var_worker_mutex);

        pthread_mutex_lock(&condition_var_controller_mutex);
        
        //If the empty variable was true change it to false so a worker thread is awakened
        pthread_mutex_lock(&full_mutex);
        full = false;
        pthread_mutex_unlock(&full_mutex);

        //Wake up all the controller theads so they complete execution
        pthread_cond_broadcast(&condition_var_controller);

        pthread_mutex_unlock(&condition_var_controller_mutex);

    }

}   

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////   WORKER THREAD   ///////////////////////////////////////////////////////////////////////////
void* worker() {

    // The worker thread constantly waits for new jobs to be issued in the buffer and then wakes up and if concurrency allows it
    // starts running a job (with fork and exec)

    while(!exit_received) {

        // Wait on the condition variable wich will be signalled when either the buffers becomes non-empty or when running < concurrency is true
        //Then if both conditions are met it will start to run a job and if not it will wait on the condition variable again
        pthread_mutex_lock(&condition_var_worker_mutex);

        pthread_mutex_lock(&empty_mutex);
        // pthread_mutex_lock(&running_mutex);
        pthread_mutex_lock(&concurrency_mutex);

        while(empty || (running >= concurrency)) {
            
            pthread_mutex_unlock(&empty_mutex);
            pthread_mutex_unlock(&running_mutex);
            pthread_mutex_unlock(&concurrency_mutex);

            pthread_cond_wait(&condition_var_worker, &condition_var_worker_mutex);

            pthread_mutex_lock(&empty_mutex);
            pthread_mutex_lock(&running_mutex);
            pthread_mutex_lock(&concurrency_mutex);
        }

        pthread_mutex_unlock(&empty_mutex);
        pthread_mutex_unlock(&running_mutex);
        pthread_mutex_unlock(&concurrency_mutex);

        pthread_mutex_unlock(&condition_var_worker_mutex);

        // If while the thread was waiting, an exit command was sent, then just break the loop and finish the worker thread execution
        if(exit_received)
            break;

        //Lock the mutexes for the buffer and the running variable so other threads can't access it while the critical section is running

        pthread_mutex_lock(&buffer_mutex);
        
        //Get the first job from the buffer
        Triplet* job_triplet = queue_remove_front(buffer);

        //If it was the last job in the buffer set empty as true
        if(buffer->size == 0) {

            pthread_mutex_lock(&empty_mutex);
            empty = true;
            pthread_mutex_unlock(&empty_mutex);

        }
        pthread_mutex_lock(&condition_var_controller_mutex);

        // If a job was succesfully removed then the buffer is not longer full so update the global variable and signal a (potential) waiting controller thread
        if(job_triplet != NULL) {

            pthread_mutex_lock(&full_mutex);
            full = false;
            pthread_mutex_unlock(&full_mutex);

            pthread_cond_signal(&condition_var_controller);

        }

        pthread_mutex_unlock(&condition_var_controller_mutex);

        //Unlock the buffer mutex so other threads can access the buffer after the job is removed
        pthread_mutex_unlock(&buffer_mutex);

        int child_pid;

        // Use fork and execvp to run the given job
        if ((child_pid = fork()) < 0) {
            fprintf(stderr,"Fork Failed\n");
            exit(1);
        }

        // If its the child proccess 
        if (child_pid == 0) {

            // Create the pid.output file for the output of the child proccess (the running job)
            // Redirect the standard output to the new file's file descriptor
            int pid = getpid() ;

            // Create the name of the output file (in the child proccess)
            char output_file[25];
            snprintf(output_file,sizeof(output_file),"%d", pid);
            strcat(output_file,".output");

            
            int file_desc;
            // Create the output file and open it 
            if ((file_desc = open(output_file, O_CREAT | O_TRUNC | O_WRONLY ,0644)) == -1) {

                fprintf(stderr, "Failed to create pid.output\n");
                exit(1);
            }
            
            // Redirect the output to the new output file
            if(dup2(file_desc,STDOUT_FILENO) < 0) {

                fprintf(stderr, "Failed to redirect output to file pid.output\n");
                exit(1);
            }

            //Break down the job into its arguments and call execvp
            int argc = 0;
            char* argv[20];
            char* token = strtok(job_triplet->job, " ");

            while(token != NULL) {

                argv[argc] = token;
                argc++;
                token = strtok(NULL, " \n");
            }

            argv[argc] = NULL;

            execvp(argv[0], argv );
            
            //If execvp failed print an error and exit
            printf("Failed to execvp : %s\n", job_triplet->job);


            exit(1);
        }
        
        // pthread_mutex_lock(&running_mutex);
        // If it is the parent (the worker thread) :
        // Increase the number of running proccesses by 1
        running++;  

        //Unlock the mutex so other threads can access the shared variables and buffer
        pthread_mutex_unlock(&running_mutex);

        //Wait for the child proccess (the proccess executing the job) to finish
        int status;
        waitpid(child_pid,&status,0);

        //Decrease the number of running proccesses by 1 (since the job just finished) (after locking the cooresponding mutex)
        pthread_mutex_lock(&running_mutex);
        running--;

        pthread_mutex_lock(&concurrency_mutex);

        pthread_mutex_lock(&condition_var_worker_mutex);

        if(running < concurrency)
            pthread_cond_signal(&condition_var_worker);

        pthread_mutex_unlock(&condition_var_worker_mutex);

        pthread_mutex_unlock(&concurrency_mutex);
        pthread_mutex_unlock(&running_mutex);

        
        // Create the name of the output file (in the parent proccess - the worker thread)
        char output_file[25];
        snprintf(output_file,sizeof(output_file),"%d", child_pid);
        strcat(output_file,".output");

        // Open the output file and read the output (which was redirected by the child proccess to the output file)
        int file_desc;

        if ((file_desc = open(output_file, O_RDONLY)) == -1) {

            fprintf(stderr, "Failed to open pid.output\n");
            exit(1);
        }

        // Read the contets of the file dynamically 
        int buff_size = 64;
        int bytes_read;
        int offset = 0;

        char* output_buffer = (char *)malloc(buff_size);

        //While there are characters in the output file read them and increase the size of the buffer whenever it fills up
        while((bytes_read = read(file_desc, output_buffer + offset , buff_size - offset)) > 0)
        {
            offset += bytes_read;

            if(buff_size <= offset) {
                buff_size *= 2;
                output_buffer = (char*)realloc(output_buffer, buff_size);
            }
        }

        //Create the structure of the output
        //Initialize a string for the "head" and "tail" of the output
        char output_start[35] = "-----";
        strcat(output_start,job_triplet->jobID);
        strcat(output_start," output start-----\n");
        
        char output_end[35] = "-----";
        strcat(output_end,job_triplet->jobID);
        strcat(output_end," output end-----\n\0");

        //Send the output back to the client
        //Write the Head of the output in the client socket
        if(write(job_triplet->clientSocket,output_start,strlen(output_start)) < 0 ) {

            fprintf(stderr,"Server: Write Failed\n");
            close(job_triplet->clientSocket);
            exit(1);
        }
        //Write the body of the output (contents of the output file) in the client socket
        if(write(job_triplet->clientSocket,output_buffer,strlen(output_buffer)) < 0 ) {

            fprintf(stderr,"Server: Write Failed\n");
            close(job_triplet->clientSocket);
            exit(1);
        }
        //Write the tail of the output in the client socket
        if(write(job_triplet->clientSocket,output_end,strlen(output_end)) < 0 ) {

            fprintf(stderr,"Server: Write Failed\n");
            close(job_triplet->clientSocket);
            exit(1);
        }

        //Write NULL (EOF) character so it doesnt read potential garbage
        if(write(job_triplet->clientSocket,"\0",1) < 0 ) {

            fprintf(stderr,"Server: Write Failed\n");
            close(job_triplet->clientSocket);
            exit(1);
        }
        
        //Shut down writting in the client socket once the output of the job has been sent
        shutdown(job_triplet->clientSocket,SHUT_WR);
        
        //Close and delete the output file
        close(file_desc);
        unlink(output_file);

        //Free the output buffer 
        free(output_buffer);

        //Close the client socket since the response was sent
        close(job_triplet->clientSocket);

        //Destroy the job triplet freeing the memory
        triplet_destroy(job_triplet);

    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////