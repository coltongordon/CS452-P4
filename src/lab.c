#include "lab.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h> /* for gettimeofday system call */
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/queue.h> // Ensure queue.h is in the same directory or adjust the include path
#define _POSIX_C_SOURCE 200809L


/**
    * @brief opaque type definition for a queue
    */
struct queue {
    int capacity; /* maximum capacity of the queue */
    int size; /* current size of the queue */
    void **data; /* data in the queue */
    int head; /* index of the head of the queue */
    int tail; /* index of the tail of the queue */
    pthread_mutex_t lock; /* mutex for locking the queue */
    pthread_cond_t not_empty; /* condition variable for not empty */
    pthread_cond_t not_full; /* condition variable for not full */
    bool shutdown; /* flag to indicate if the queue is shutting down */
};

typedef struct queue *queue_t;


/**
    * @brief Initialize a new queue
    *
    * @param capacity the maximum capacity of the queue
    * @return A fully initialized queue
    */

queue_t queue_init(int capacity) {
    // Ensure the capacity is valid (greater than 0)
    if (capacity <= 0)
        return NULL;

    // Allocate memory for the queue structure
    queue_t q = malloc(sizeof(struct queue));
    if (q == NULL) {
        return NULL;
    }
    if (q->data == NULL) {
        free(q);
        return NULL;
    }

    // Initialize the queue's fields
    q->capacity = capacity; // Set the maximum capacity
    q->size = 0;            // Initially, the queue is empty
    q->head = 0;            // Head starts at index 0
    q->tail = 0;            // Tail starts at index 0
    q->shutdown = false;    // Queue is not in shutdown mode

    // Allocate memory for the data array
    q->data = malloc(sizeof(void *) * capacity);
    if (q->data == NULL) {
        // If allocation fails, free the queue structure and return NULL
        free(q);
        return NULL;
    }

    // Initialize the mutex and condition variables
    pthread_mutex_init(&q->lock, NULL);       // Mutex for thread safety
    pthread_cond_init(&q->not_empty, NULL);  // Condition variable for "not empty"
    pthread_cond_init(&q->not_full, NULL);   // Condition variable for "not full"
    free(q->data); // Free the data pointer to avoid memory leak
    free(q); // Free the queue structure to avoid memory leak
    // Return the fully initialized queue
    return q;
}


/**
    * @brief Frees all memory and related data signals all waiting threads.
    *
    * @param q a queue to free
    */
void queue_destroy(queue_t q) {
    if (q == NULL) // Check if the queue is NULL, nothing to destroy
        return;

    // Free the memory allocated for the data array
    free(q->data);

    // Free the memory allocated for the queue structure
    free(q);

    // Set the queue pointer to NULL to avoid dangling pointers
    q = NULL;

    // Destroy the mutex to release resources
    pthread_mutex_destroy(&q->lock);

    // Destroy the condition variables to release resources
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);

    return; // Return after cleanup
}


   /**
    * @brief Adds an element to the back of the queue
    *
    * @param q the queue
    * @param data the data to add
    */
void enqueue(queue_t q, void *data){
    // Check if the queue or data is NULL, if so, return immediately
    if (q == NULL || data == NULL)
        return;

    // Lock the queue to ensure thread safety
    pthread_mutex_lock(&q->lock);

    // Wait until there is space in the queue or the queue is shutting down
    while (q->size == q->capacity && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    // If the queue is shutting down, unlock and return
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return;
    }

    // Add the data to the tail of the queue
    q->data[q->tail] = data;

    // Update the tail index, wrapping around if necessary
    q->tail = (q->tail + 1) % q->capacity;

    // Increment the size of the queue
    q->size++;

    // Signal any waiting threads that the queue is not empty
    pthread_cond_signal(&q->not_empty);

    // Unlock the queue
    pthread_mutex_unlock(&q->lock);

    // Return after successfully enqueuing the data
    return;
}


/**
    * @brief Removes the first element in the queue.
    *
    * @param q the queue
    */
void *dequeue(queue_t q) {
    // Check if the queue is NULL, if so, return NULL
    if (q == NULL)
        return NULL;

    // Lock the queue to ensure thread safety
    pthread_mutex_lock(&q->lock);

    // Wait until there is data in the queue or the queue is shutting down
    while (q->size == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    // If the queue is shutting down, unlock and return NULL
    if (q->shutdown && q->size == 0) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    // Retrieve the data from the head of the queue
    void *data = q->data[q->head];

    // Update the head index, wrapping around if necessary
    q->head = (q->head + 1) % q->capacity;

    // Decrement the size of the queue
    q->size--;

    // Signal any waiting threads that the queue is not full
    pthread_cond_signal(&q->not_full);

    // Unlock the queue
    pthread_mutex_unlock(&q->lock);

    // Return the dequeued data
    return data;
}


   /**
    * @brief Set the shutdown flag in the queue so all threads can
    * complete and exit properly
    *
    * @param q The queue
    */
void queue_shutdown(queue_t q) {
    // Check if the queue is NULL, if so, return immediately
    if (q == NULL)
        return;

    // Lock the queue to ensure thread safety
    pthread_mutex_lock(&q->lock);

    // Set the shutdown flag to true
    q->shutdown = true;

    // Broadcast to all waiting threads that the queue is shutting down
    pthread_cond_broadcast(&q->not_empty); // Notify threads waiting for the queue to be not empty
    pthread_cond_broadcast(&q->not_full); // Notify threads waiting for the queue to be not full

    // Unlock the queue
    pthread_mutex_unlock(&q->lock);

    // Return after setting the shutdown flag and notifying threads
    return;
}


    /**
    * @brief Returns true is the queue is empty
    *
    * @param q the queue
    */
bool is_empty(queue_t q) {
    // Check if the queue is NULL, if so, return false
    if (q == NULL)
        return false;

    // Lock the queue to ensure thread safety
    pthread_mutex_lock(&q->lock);

    // Check if the size of the queue is 0 (empty)
    bool empty = (q->size == 0);

    // Unlock the queue after checking
    pthread_mutex_unlock(&q->lock);

    // Return whether the queue is empty
    return empty;
}


   /**
    * @brief
    *
    * @param q The queue
    */
bool is_shutdown(queue_t q){
    // Check if the queue is NULL, if so, return false
    if (q == NULL)
        return false;

    // Lock the queue to ensure thread safety
    pthread_mutex_lock(&q->lock);

    // Retrieve the current value of the shutdown flag
    bool shutdown = q->shutdown;

    // Unlock the queue after checking the shutdown flag
    pthread_mutex_unlock(&q->lock);

    // Return whether the queue is in shutdown mode
    return shutdown;
}