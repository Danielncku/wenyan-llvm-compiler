#pragma once
#ifndef __WJCL_LINKED_LIST_H__
#define __WJCL_LINKED_LIST_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "../memory/wjcl_mem_track_util.h"

typedef struct LinkedListNode {
    struct LinkedListNode* prev;
    struct LinkedListNode* next;
    void* value;
    // Flag for freeing value
    uint8_t freeFlag;
} LinkedListNode;

typedef struct LinkedList {
    size_t length;
    LinkedListNode* head;
} LinkedList;

#define linkedList_create() \
    { 0, NULL }

/**
 * List should be init after creation, and can only use once
 * @param listptr LinkedList pointer
*/
static inline void linkedList_init(LinkedList* listptr) {
    if (!listptr) return;

    LinkedListNode* node = (LinkedListNode*)__malloc(sizeof(LinkedListNode));
    if (!node) return;

    *node = (LinkedListNode){
        .next = node,
        .prev = node,
        .value = NULL,
        .freeFlag = 0
    };

    listptr->head = node;
    listptr->length = 0;
}

static inline LinkedList* linkedList_new() {
    LinkedList* list = (LinkedList*)__malloc(sizeof(LinkedList));
    linkedList_init(list);
    return list;
}

/**
 * @brief Add a value to linked list
 * @param list LinkedList pointer
 * @param value Value
 */
#define linkedList_add(list, value) *(typeof(value)*)linkedList_addp(list, 1, __malloc(sizeof(typeof(value)))) = value

/**
 * @brief Add a pointer to linked list
 * @param list LinkedList pointer
 * @param value Value pointer (pointer will not be freed when the node is deleted)
 */
#define linkedList_addPtr(list, value) linkedList_addp(list, 0, value)

#define linkedList_get(list, type, index) *(type*)linkedList_getNode(list, index)->value

#define linkedList_getPtr(list, index) linkedList_getNode(list, index)->value

#define linkedList_foreach(list, node) \
    for (LinkedListNode* node = (list)->head->next; \
         node != (list)->head; \
         node = node->next)

#define linkedList_foreach_safe(list, node, n) \
    for (LinkedListNode *node = (list)->head->next, *n = node->next; \
         node != (list)->head; \
         node = n, n = node->next)

#define linkedList_nodeVal(type, node) *(type*)node->value

#define linkedList_clear(list) linkedList_clearA(list, NULL)

/**
 * List should not be used after free
 * @param list LinkedList pointer
 */
#define linkedList_free(list) linkedList_freeA(list, NULL)

// Method
void linkedList_appendNode(LinkedList* list, LinkedListNode* node);
void linkedList_deleteNode(LinkedList* list, LinkedListNode* node);
void linkedList_removeNode(LinkedList* list, LinkedListNode* node);
void linkedList_clearA(LinkedList* list, void (*freeValue)(void* value));
void linkedList_freeA(LinkedList* list, void (*freeValue)(void* value));

void* linkedList_addp(LinkedList* list, uint8_t freeFlag, void* value);

// Implementation
// #define WJCL_LINKED_LIST_IMPLEMENTATION
#ifdef WJCL_LINKED_LIST_IMPLEMENTATION

void* linkedList_addp(LinkedList* list, uint8_t freeFlag, void* value) {
    LinkedListNode* node = (LinkedListNode*)__malloc(sizeof(LinkedListNode));
    node->freeFlag = freeFlag;
    node->value = value;

    node->next = list->head;
    node->prev = list->head->prev;
    list->head->prev->next = node;
    list->head->prev = node;

    ++(list->length);
    return value;
}

void linkedList_appendNode(LinkedList* list, LinkedListNode* node) {
    node->next = list->head;
    node->prev = list->head->prev;
    list->head->prev->next = node;
    list->head->prev = node;

    ++(list->length);
}

LinkedListNode* linkedList_getNode(LinkedList* list, size_t index) {
    if (index >= list->length) return NULL;
    LinkedListNode* node;
    if (index > list->length / 2) {
        node = list->head->prev;
        for (size_t i = list->length - 1; i > index; --i)
            node = node->prev;
    } else {
        node = list->head->next;
        for (size_t i = 0; i < index; ++i)
            node = node->next;
    }
    return node;
}

void linkedList_removeNode(LinkedList* list, LinkedListNode* node) {
    if (!node || node == list->head) return;

    node->prev->next = node->next;
    node->next->prev = node->prev;
    --(list->length);
}

/**
 * @brief Free node value if flag set, and free node
 * @param list
 * @param node
 */
void linkedList_deleteNode(LinkedList* list, LinkedListNode* node) {
    if (!node || node == list->head) return;

    node->prev->next = node->next;
    node->next->prev = node->prev;
    --(list->length);
    if (node->freeFlag) {
        free(node->value);
    }
    __free(node);
}

void linkedList_clearA(LinkedList* list, void (*freeValue)(void* value)) {
    if (!list) return;
    LinkedListNode* node = list->head->next;
    while (node != list->head) {
        LinkedListNode* nextNode = node->next;
        if (freeValue)
            freeValue(node->value);
        if (node->freeFlag)
            free(node->value);
        __free(node);
        node = nextNode;
    }
    list->head->next = list->head->prev = list->head;
    list->length = 0;
}

void linkedList_freeA(LinkedList* list, void (*freeValue)(void* value)) {
    if (!list) return;
    LinkedListNode* node = list->head->next;
    while (node != list->head) {
        LinkedListNode* nextNode = node->next;
        if (freeValue)
            freeValue(node->value);
        if (node->freeFlag)
            free(node->value);
        __free(node);
        node = nextNode;
    }
    __free(list->head);
}

#endif /* WJCL_LINKED_LIST_IMPLEMENTATION */

#endif
