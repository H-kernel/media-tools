#ifndef __AS_MEDIA_KENERL_BUFFER_QUEUE_H__
#define __AS_MEDIA_KENERL_BUFFER_QUEUE_H__
#include "as_config.h"
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if AS_APP_OS == AS_OS_WIN32
#include <windows.h>
#endif
#include "as_mutex.h"

typedef struct as_queue_s  as_queue_t;

struct as_queue_s {
    as_queue_t  *prev;
    as_queue_t  *next;
};


#define as_queue_init(q)                                                      \
    (q)->prev = q;                                                            \
    (q)->next = q


#define as_queue_empty(h)                                                     \
    (h == (h)->prev)


#define as_queue_insert_head(h, x)                                            \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x


#define as_queue_insert_after   as_queue_insert_head


#define as_queue_insert_tail(h, x)                                            \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x


#define as_queue_head(h)                                                      \
    (h)->next


#define as_queue_last(h)                                                      \
    (h)->prev


#define as_queue_sentinel(h)                                                  \
    (h)


#define as_queue_next(q)                                                      \
    (q)->next


#define as_queue_prev(q)                                                      \
    (q)->prev


#if (AS_DEBUG)

#define as_queue_remove(x)                                                    \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next;                                              \
    (x)->prev = NULL;                                                         \
    (x)->next = NULL

#else

#define as_queue_remove(x)                                                    \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#endif


#define as_queue_split(h, q, n)                                               \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;


#define as_queue_add(h, n)                                                    \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;


#define as_queue_data(q, type, link)                                          \
    (type *) ((u_char *) q - offsetof(type, link))


as_queue_t *as_queue_middle(as_queue_t *queue);
void as_queue_sort(as_queue_t *queue,
    int32_t (*cmp)(const as_queue_t *, const as_queue_t *));



#endif /* __AS_MEDIA_KENERL_BUFFER_QUEUE_H__ */
