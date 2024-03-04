#ifndef ARRAY_QUEUE_H
#define ARRAY_QUEUE_H

#include <stddef.h>
#include <stdlib.h> 
#include <stdio.h> 
#include <stdint.h>
#include <string.h>

typedef struct {
    void* memory;
    struct Node* next;
    struct Node* previous;
} Node;

typedef struct {
    Node* nodes;
    Node* head;
    Node* tail;
    uint64_t capacity;
} ArrayQueue;

void arrayQueue_init(ArrayQueue* queue, uint64_t capacity) 
{
    queue->head = NULL;
    queue->tail = NULL;

    queue->nodes = (Node*) malloc(capacity*sizeof(Node));    
    memset(queue->nodes, 0, capacity*sizeof(Node));

    queue->capacity = capacity;
}

void arrayQueue_deinit(ArrayQueue* queue) 
{
    queue->head = NULL;
    queue->tail = NULL;

    Node* node = queue->nodes;
    if (node == NULL) return;
    for (int i = 0; i < queue->capacity; i++)
    {
        if (node->memory != NULL)
        {
            free(node->memory);
            node->memory = NULL;
        }

        node++;
    }
    

    free(queue->nodes);
    queue->nodes = NULL;
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
    if (queue->head == node) return;
    
    Node* formerPrevious = node->previous;
    Node* formerNext = node->next;
    Node* formerHead = queue->head;

    queue->head = node;
    node->previous = NULL;
    node->next = formerHead;

    if (queue->tail == NULL)
    {
        queue->tail = node;
    }

    if (formerHead != NULL)
    {        
        formerHead->previous = node;
    }    

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
    if (queue->tail == NULL) return;

    Node* formerTail = queue->tail;

    queue->tail = queue->tail->previous;
    if (queue->tail != NULL)
    {
        queue->tail->next = NULL;
    }    

    formerTail->memory = NULL;
    formerTail->previous = NULL;
    formerTail->next = NULL;
}

#endif