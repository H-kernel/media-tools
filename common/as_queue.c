#include "as_thread.h"
#include "as_config.h"
#include "as_common.h"
#if AS_APP_OS == AS_OS_LINUX
#include <pthread.h>
#elif AS_APP_OS == AS_OS_WIN32
#endif
#include "as_queue.h"

as_queue_t *
as_queue_middle(as_queue_t *queue)
{
    as_queue_t  *middle, *next;

    middle = as_queue_head(queue);

    if (middle == as_queue_last(queue)) {
        return middle;
    }

    next = as_queue_head(queue);

    for ( ;; ) {
        middle = as_queue_next(middle);

        next = as_queue_next(next);

        if (next == as_queue_last(queue)) {
            return middle;
        }

        next = as_queue_next(next);

        if (next == as_queue_last(queue)) {
            return middle;
        }
    }
}


/* the stable insertion sort */

void
as_queue_sort(as_queue_t *queue,
    int32_t (*cmp)(const as_queue_t *, const as_queue_t *))
{
    as_queue_t  *q, *prev, *next;

    q = as_queue_head(queue);

    if (q == as_queue_last(queue)) {
        return;
    }

    for (q = as_queue_next(q); q != as_queue_sentinel(queue); q = next) {

        prev = as_queue_prev(q);
        next = as_queue_next(q);

        as_queue_remove(q);

        do {
            if (cmp(prev, q) <= 0) {
                break;
            }

            prev = as_queue_prev(prev);

        } while (prev != as_queue_sentinel(queue));

        as_queue_insert_after(prev, q);
    }
}


