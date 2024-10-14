#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"


Triplet* triplet_create(char* jobID, char* job, int clientSocket ) {

    // Allocate memory for the new triplet
    Triplet* triplet = malloc(sizeof(Triplet));
    
    triplet->jobID = (char*)malloc(strlen(jobID));
    triplet->job = (char*)malloc(strlen(job));

    //Copy the jobID and the job to the new triplet
    strcpy(triplet->jobID,jobID);
    strcpy(triplet->job, job);
    
    //Copy the client socket
    triplet->clientSocket = clientSocket;
    
    return triplet;

}   

void triplet_destroy(Triplet* triplet) {

    //Free the jobID and job strings
    free(triplet->job);
    free(triplet->jobID);

    free(triplet);
}

Queue* queue_create() {

    //Allocate memory for the queue
    Queue* queue = (Queue*)malloc(sizeof(Queue));   

    //Initialize the front and rear pointers to be NULL (as the queue is initialized empty) and the size to 0
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;

    return queue;

}

void queue_insert_rear(Queue* queue , Triplet* triplet) {
    
    // Allocate memory for the new node
    Node* node = (Node*)malloc(sizeof(Node));

    // Initialize the node accordingly 
    node->next = NULL;
    node->triplet = triplet;
    
    // If the queue is empty ( front = NULL ) front as the new node
    if (queue->front == NULL) 
        queue->front = node;
    
    //If the queue is not empty set the previous rear's next as the new node
    else 
        queue->rear->next = node;
    
    // Rear will now point to the new node 
    queue->rear = node;
    //Increase the size of the queue by 1
    queue->size++;

}

Triplet* queue_remove_front(Queue* queue) {

    //If the queue is empty return NULL
    if(queue->front == NULL) {
        return NULL;
    }

    Node* to_remove = queue->front;

    //Set the next node of the front as the new front 
    queue->front = to_remove->next;
    
    //If there was only 1 element in the queue after removing it set the rear pointer to NULL as well
    if(queue->front == NULL)
        queue->rear = NULL;

    // Get the pair of the front node 
    Triplet* triplet = to_remove->triplet;

    //Free the node removed
    free(to_remove);

    //Decrease the queue size by 1
    queue->size--;

    return triplet;
        
}

Triplet* queue_remove_jobID(Queue* queue,char* job_ID) {

        //Initialize 2 node pointers to get the node that has to be removed and its previous one (to modify its "next" pointer)
        Node* previous_node = NULL;
        Node* node;

        // Access the nodes (from front to rear) until either the node with that job_ID is found or the end of the queue is reached
        for( node = queue->front ; node != NULL ; node = node->next)
        {
            if(strcmp(job_ID,node->triplet->jobID) == 0) {
                break;
            }
            previous_node = node;
        }

        //If the job_ID wasn't found return NULL
        if(node == NULL) {
            return NULL;
        }

        //If previous node is not NULL make the previous node's next, point to the next node of the one being removed.
        if(previous_node != NULL)
            previous_node->next = node->next;
        
        //If the previous node is NULL it means the first one is the one being removed so set front as the next of the one being removed.
        else 
            queue->front = node->next;

        //Finally if it is the rear node that has to be removed (and the list has more than 1 elements) then make rear point to the previous node 
        if(strcmp(queue->rear->triplet->jobID, job_ID) == 0 ) 
            queue->rear = previous_node;

        //Find the pair that has to be removed
        Triplet* triplet = node->triplet;

        //Free the cooresponding node 
        free(node);

        //Decrease the queue size by 1
        queue->size--;

        return triplet;
}

void queue_destroy(Queue* queue) {

    Node* next;
    // Access all the queue nodes from front to rear and destroy them one by one freeing the memory
    for(Node* node = queue->front; node != NULL ; node = next ) {
        
        next = node->next;
        triplet_destroy(node->triplet);

        free(node);
    }

    free(queue);
}
