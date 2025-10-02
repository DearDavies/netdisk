#include "queue.h"

// 初始化队列
void init_queue(queue_t* queue) {
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;

    if (pthread_mutex_init(&queue->mutex_queue, NULL) != 0) {
        perror("pthread_mutex_init 初始化队列的互斥锁出错");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&queue->cond_new_fd, NULL) != 0) {
        perror("pthread_cond_init 初始化队列的条件变量出错");
        exit(EXIT_FAILURE);
    }
}

// 销毁队列
void destroy_queue(queue_t* queue) {
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    if (pthread_mutex_destroy(&queue->mutex_queue) != 0) {
        perror("pthread_mutex_destroy 销毁队列的互斥锁出错");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&queue->cond_new_fd) != 0) {
        perror("pthread_cond_destroy 销毁队列的条件变量出错");
        exit(EXIT_FAILURE);
    }
}

// 将 fd 入队到队列中
void enqueue(queue_t* queue, int fd) {
    node_t* new_node = (node_t*)calloc(1, sizeof(node_t));
    new_node->fd = fd;
    pthread_mutex_lock(&queue->mutex_queue);
    if (queue->size == 0) {
        queue->head = new_node;
        queue->tail = new_node;
    }else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    queue->size++;
    pthread_cond_signal(&queue->cond_new_fd);
    pthread_mutex_unlock(&queue->mutex_queue);
}

// 从队列中出队，返回 fd
int dequeue(queue_t* queue) {
    node_t* temp = NULL;
    pthread_mutex_lock(&queue->mutex_queue);
    while (queue->size <= 0) {
        pthread_cond_wait(&queue->cond_new_fd, &queue->mutex_queue);
    }
    temp = queue->head;
    if (queue->size == 1) {
        queue->head = NULL;
    }else {
        queue->head = queue->head->next;
    }
    queue->size--;
    pthread_mutex_unlock(&queue->mutex_queue);
    int fd = temp->fd;
    free(temp);
    return fd;
}