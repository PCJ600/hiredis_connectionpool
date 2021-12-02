#ifndef __LIST_H__
#define __LIST_H__

typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

typedef struct list {
    listNode *head;
    listNode *tail;
    void *(*free)(void *ptr);
    int len;
} list;


list *listCreate(void);
void listRelease(list *list);
void *listPopFront(list *list);
void listPushBack(list *list, void *node);
int listisEmpty(list *list);

#endif
