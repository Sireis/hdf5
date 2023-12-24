#ifndef ARRAY_QUEUE_H
#define ARRAY_QUEUE_H

#include <stddef.h>
#include <stdlib.h> 
#include <stdio.h> 
#include <stdint.h>
#include <string.h>

typedef struct Node {
    void* memory;
    struct Node* next;
    struct Node* previous;
} Node;

typedef struct {
    Node* nodes;
    Node* head;
    Node* tail;
} ArrayQueue;

void arrayQueue_init(ArrayQueue* queue, uint64_t capacity) 
{
    queue->head = NULL;
    queue->tail = NULL;

    queue->nodes = (Node*) malloc(capacity*sizeof(Node));
    memset(queue->nodes, 0, capacity*sizeof(Node));
}

void arrayQueue_deinit(ArrayQueue* queue) 
{
    queue->head = NULL;
    queue->tail = NULL;

    free(queue->nodes);
}

Node* arrayQueue_get_by_index(ArrayQueue* queue, uint64_t i)
{   
    return &(queue->nodes[i]);
}

Node* arrayQueue_get_tail(ArrayQueue* queue)
{
    return queue->tail;
}

void arrayQueue_move_to_front(ArrayQueue* queue, Node* node) 
{
    Node* formerPrevious = node->previous;
    Node* formerNext = node->next;
    Node* formerHead = queue->head;

    queue->head = node;
    node->previous = NULL;
    node->next = formerHead;

    if (formerPrevious != NULL)
    {
        formerPrevious->next = formerNext;
    }

    if (formerNext != NULL)
    {
        formerNext->previous = formerPrevious;
    }
}

void arrayQueue_pop_tail(ArrayQueue* queue)
{
    Node* formerTail = queue->tail;

    queue->tail = formerTail->previous;
    queue->tail->next = NULL;

    formerTail->memory = NULL;
    formerTail->previous = NULL;
    formerTail->next = NULL;
}

#endif