#include "list.h"
#include <stdlib.h>

list *listCreate(void)
{
    struct list *list;
    if ((list = malloc(sizeof(*list))) == NULL) {
        return NULL;
    }
    list->head = list->tail = NULL;
    list->len = 0;
    list->free = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
void listEmpty(list *list)
{
    int len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        if (list->free) list->free(current->value);
        free(current);
        current = next;
    }
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
void listRelease(list *list)
{
    listEmpty(list);
    free(list);
}

void *listPopFront(list *list)
{
    if (list->len == 0) {
        return NULL;
    }
    void *value = list->head->value;

    listNode *node = list->head;
    list->head = node->next;
    if (node->next) {
        node->next->prev = NULL;
    } else {
        list->tail = NULL;
    }
    free(node);
    list->len--;
    return value;
}

void listPushBack(list *list, void *value)
{
    listNode *node;

    if ((node = malloc(sizeof(*node))) == NULL) {
        return;
    }
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;
}

int listisEmpty(list *list)
{
    return (list->len == 0);
}
