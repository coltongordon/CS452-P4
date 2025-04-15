#include <lab.h>
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


/**
    * @brief opaque type definition for a queue
    */
typedef struct queue *queue_t{
    int capacity; /* maximum capacity of the queue */
    int size; /* current size of the queue */
    void **data; /* data in the queue */
    int head; /* index of the head of the queue */
    int tail; /* index of the tail of the queue */
    pthread_mutex_t lock; /* mutex for locking the queue */
    pthread_cond_t not_empty; /* condition variable for not empty */
    pthread_cond_t not_full; /* condition variable for not full */
    bool shutdown; /* flag to indicate if the queue is shutting down */
} queue_t;


/**
    * @brief Initialize a new queue
    *
    * @param capacity the maximum capacity of the queue
    * @return A fully initialized queue
    */
   queue_t queue_init(int capacity){
    if (capacity <= 0)
        return NULL;
    queue_t q = malloc(sizeof(struct queue));
    if (q == NULL)
        return NULL;
    q->capacity = capacity;
    q->size = 0;
    q->head = 0;
    q->tail = 0;
    q->shutdown = false;
    q->data = malloc(sizeof(void *) * capacity);
    if (q->data == NULL) {
        free(q);
        return NULL;
    }
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return q;
   }

/**
    * @brief Frees all memory and related data signals all waiting threads.
    *
    * @param q a queue to free
    */
   void queue_destroy(queue_t q){
    if (q == NULL)
        return;
    free(q->data);
    free(q);
    q = NULL;
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    return;
   }

   /**
    * @brief Adds an element to the back of the queue
    *
    * @param q the queue
    * @param data the data to add
    */
   void enqueue(queue_t q, void *data){
    if (q == NULL || data == NULL)
        return;
    pthread_mutex_lock(&q->lock);
    while (q->size == q->capacity && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return;
    }
    q->data[q->tail] = data;
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return;
   }

/**
    * @brief Removes the first element in the queue.
    *
    * @param q the queue
    */
   void *dequeue(queue_t q){
    if (q == NULL)
        return NULL;
    pthread_mutex_lock(&q->lock);
    while (q->size == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    void *data = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return data;
   }

   /**
    * @brief Set the shutdown flag in the queue so all threads can
    * complete and exit properly
    *
    * @param q The queue
    */
   void queue_shutdown(queue_t q){
    if (q == NULL)
        return;
    pthread_mutex_lock(&q->lock);
    q->shutdown = true;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return;
   }

    /**
    * @brief Returns true is the queue is empty
    *
    * @param q the queue
    */
   bool is_empty(queue_t q){
    if (q == NULL)
        return false;
    pthread_mutex_lock(&q->lock);
    bool empty = (q->size == 0);
    pthread_mutex_unlock(&q->lock);
    return empty;
   }

   /**
    * @brief
    *
    * @param q The queue
    */
   bool is_shutdown(queue_t q){
    if (q == NULL)
        return false;
    pthread_mutex_lock(&q->lock);
    bool shutdown = q->shutdown;
    pthread_mutex_unlock(&q->lock);
    return shutdown;
   }