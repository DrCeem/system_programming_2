#include <stdio.h>

// A struct to store the jobs with their respective ids and client sockets
typedef struct triplet {

    char* jobID;
    char* job;
    int clientSocket;

} Triplet;

// A struct used to represent the node of a queue containing a triplet pointer and a pointer to the next node in the queue
typedef struct queue_node {

    Triplet* triplet;
    struct queue_node* next;

} Node;

// A struct used to represent the queue containing a front and a rear queue node pointer as well as an integer for the queue's size
typedef struct queue {
    Node* front;
    Node* rear;
    int size;
} Queue;

// A function that takes the jobID, the job (as strings) and the client socket and creates a triplet returning a pointer to it
Triplet* triplet_create(char* jobID, char* job, int clientSocket );

// A function that takes a triplet (pointer) and destroys it freeing the memory
void triplet_destroy(Triplet* triplet);

// A function that creates an empty queue returning a pointer to it
Queue* queue_create();

// A function that takes a queue and a triplet pointer and inserts the triplet at the end of the queue
void queue_insert_rear(Queue* queue , Triplet* triplet);

// A function that takes a queue and removes the first element returning a pointer to it. If the queue is empty returns NULL
Triplet* queue_remove_front(Queue* queue);

// A function that takes a queue pointer and a jobID (string) and removes the triplet with that jobID returning a pointer to it.
//If the jobID doesn't exist returns NULL.
Triplet* queue_remove_jobID(Queue* queue,char* job_ID) ;

// A function that takes a queue pointer and destroys the queue freeing all the memory
void queue_destroy(Queue* queue) ;
