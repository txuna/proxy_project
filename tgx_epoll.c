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

tgx_err_t eventloop_delevent(struct tgx_eventloop *eventloop, struct tgx_file *file)
{
    if(file == NULL)
    {
        return TGX_DEL_EVENT_ERROR;
    }

    struct epoll_event ee = {0};
    ee.data.fd = file->fd; 
    ee.events = file->mask;

    if(epoll_ctl(eventloop->epfd, EPOLL_CTL_DEL, file->fd, &ee) < 0)
    {
        return TGX_DEL_EVENT_ERROR;
    }

    close(file->fd);
    list_del(&file->list);
    free(file);
    return TGX_OK;
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

void tcp_network_process(void *arg)
{
    struct tgx_file *file = (struct tgx_file*)arg;
    int can_read_len; 
    int size = SOCK_BUFFER;
    char *buffer = calloc(1, sizeof(char) * SOCK_BUFFER);
    int nsize = 0;
    int total_size = 0;
    /* 여기서 끝내면 EPOLLET라서 데이터를 못 읽는데 조금 더 우아하게 끝내는 법 찾기 */
    if(buffer == NULL)
    {
        return;
    }
    
    while(true)
    {
        nsize = read(file->fd, buffer, size);
        if(nsize < 0)
        {
            if(errno == EAGAIN)
            {
                continue;
            }
            
            printf("read errno : %d\n", errno);
            break;
        }
        total_size += nsize;
        /**
         * 더 읽을 버퍼가 있다는 뜻
        */
        if(nsize == SOCK_BUFFER)
        {
            if(socket_nread(file->fd, &can_read_len) == -1)
            {
                break;
            }

            total_size += can_read_len;
            // NEED TO CHECK NULL;
            buffer = realloc(buffer, sizeof(char) * total_size);
            if(buffer == NULL)
            {
                return;
            }
            size = can_read_len;
            continue;
        }

        break;
    }

    while(true)
    {
        int ret = write(file->sock.v.pair_fd, buffer, total_size);
        if(ret < 0)
        {
            if(errno == EAGAIN)
            {
                continue;
            }

            printf("send errno : %d\n", errno);
            break;
        }
        
        break;
    }

    free(buffer);
    return;
}

tgx_err_t tcp_process(struct tgx_eventloop *eventloop, 
                            struct tgx_file *listener, 
                            sds host, int port)
{
    int accept_fd, connect_fd; 

    if(net_tcp_accept(&accept_fd, listener->fd) != TGX_OK)
    {
        return TGX_SOCK_ERROR;
    }

    struct tgx_file *accept_file = file_alloc(accept_fd, EPOLLET | EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR, TGX_TCP);
    if(accept_file == NULL)
    {
        close(accept_fd);
        return TGX_SOCK_ERROR;
    }

    if(eventloop_addevent(eventloop, accept_file) != TGX_OK)
    {
        close(accept_fd);
        free(accept_file);
        return TGX_SOCK_ERROR;
    }

    if(net_tcp_connect(&connect_fd, host, port) != TGX_OK)
    {
        return TGX_SOCK_ERROR;
    }

    struct tgx_file *connect_file = file_alloc(connect_fd, EPOLLET | EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR, TGX_TCP);
    if(connect_file == NULL)
    {
        eventloop_delevent(eventloop, accept_file);
        close(connect_fd);
        return TGX_SOCK_ERROR;
    }

    if(eventloop_addevent(eventloop, connect_file) != TGX_OK)
    {
        eventloop_delevent(eventloop, accept_file);
        close(connect_fd);
        free(connect_file);
        return TGX_SOCK_ERROR;
    }

    accept_file->sock.is_listen = false;
    connect_file->sock.is_listen = false;

    accept_file->sock.v.pair_fd = connect_file->fd;
    connect_file->sock.v.pair_fd = accept_file->fd;

    printf("accept fd : %d\n", accept_fd);
    printf("connect fd : %d\n", connect_fd);

    return TGX_OK;
}