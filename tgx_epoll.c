#include "tgx_epoll.h"

struct tgx_eventloop *eventloop_alloc(int timeout)
{
    struct tgx_eventloop *eventloop = calloc(1, sizeof(struct tgx_eventloop));
    if(eventloop == NULL)
    {
        return NULL;
    }

    eventloop->epfd = epoll_create(1024); 
    if(eventloop->epfd == -1)
    {
        goto err;
    }

    eventloop->maxsize = MAX_EPOLL_SIZE;
    eventloop->timeout = timeout;

    /* 파일이벤트 관리 리스트 */
    INIT_LIST_HEAD(&eventloop->events);
    return eventloop;

err:
    close(eventloop->epfd);
    free(eventloop);
    return NULL;
}

tgx_err_t eventloop_addevent(struct tgx_eventloop *eventloop, struct tgx_file *file)
{
    struct epoll_event ee = {0};
    
    ee.events = file->mask;
    ee.data.ptr = (void*)file;

    if(epoll_ctl(eventloop->epfd, EPOLL_CTL_ADD, file->fd, &ee) == -1)
    {
        return TGX_ADD_EVENT_ERROR;
    }

    list_add_tail(&file->list, &eventloop->events);

    return TGX_OK;
}

int eventloop_poll(struct tgx_eventloop *eventloop)
{
    struct epoll_event ees[eventloop->maxsize];

    int retval = epoll_wait(eventloop->epfd, ees, eventloop->maxsize, eventloop->timeout);

    if(retval <= 0)
    {
        return retval;
    }

    eventloop->fired = (struct tgx_file**)calloc(retval, sizeof(struct tgx_file*));
    if(eventloop->fired == NULL)
    {
        return 0; 
    }

    for(int i=0; i<retval; i++)
    {
        struct epoll_event *e = (ees + i);
        eventloop->fired[i] = e->data.ptr;
        eventloop->fired[i]->fire_event = e->events;
    }

    return retval;
}

/**
 * type : 해당 파일이 TCP or UDP or PIPE FD인지 확인 
*/
struct tgx_file *file_alloc(int fd, int mask, int type)
{
    struct tgx_file *file = calloc(1, sizeof(struct tgx_file));
    if(file == NULL)
    {
        return NULL;
    }

    file->fd = fd; 
    file->mask = mask; 
    file->type = type;

    /* set READ and WRITE Handler */

    return file;
}