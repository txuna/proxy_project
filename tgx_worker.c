#include "tgx_worker.h"

#include <stdio.h>

struct tgx_server *global_server = NULL;
struct tgx_eventloop *global_eventloop = NULL;

static void signal_clean(int sig)
{
    if(global_eventloop != NULL)
    {
        free_eventloop(global_eventloop);
    }
    if(global_server != NULL)
    {
        free_server(global_server);
    }
    exit(0);
}

/**
 * MUST FREE SERVER ALSO IN CHILD PROCESS!!
 * worker process는 상대 프록시 서버로부터 패킷이 온다면 자신의 클라우드에서 서비스중인 
 * 서버에게 전달하는 리버스 프록시 
 * 여기는 라우팅 테이블이 필요없네 
*/
void tgx_worker_process(struct tgx_server *server, int index)
{
    struct tgx_service service = server->service[index];
    struct tgx_eventloop *eventloop = eventloop_alloc(INFINITE);
    int listener_fd; 

    if(eventloop == NULL)
    {
        goto fin;
    }

    global_server = server;
    global_eventloop = eventloop;

    signal(SIGTERM, signal_clean);

    if(do_pipe(server, eventloop, index) != TGX_OK)
    {
        goto event_clear;
    }

    if(do_bind(server, eventloop, index) != TGX_OK)
    {
        goto event_clear;
    }

    eventloop->thpool = thpool_init(MAX_THREAD_POOL);

    tgx_worker_poll(server, eventloop, index);

event_clear:
    free_eventloop(eventloop);

fin:
    free_server(server);
    return;
}

tgx_err_t do_pipe(struct tgx_server *server, struct tgx_eventloop *eventloop, int index)
{
    for(int i=0; i<server->service_len; i++)
    {
        int *pipe = server->pipes[i];
        if(i == index)
        {
            close(pipe[in]);
            continue;
        }

        close(pipe[out]);
        close(pipe[in]);
    }

    struct tgx_file *file = file_alloc(server->pipes[index][out], EPOLLIN, TGX_PIPE);
    if(file == NULL)
    {
        close(server->pipes[index][out]);
        return TGX_ALLOC_ERROR;
    }

    if(eventloop_addevent(eventloop, file) != TGX_OK)
    {
        close(server->pipes[index][out]);
        free(file);
        return TGX_ALLOC_ERROR;
    }

    return TGX_OK;
}

tgx_err_t do_bind(struct tgx_server *server, struct tgx_eventloop *eventloop, int index)
{
    struct tgx_service service = server->service[index];
    int listener_fd;
    tgx_err_t err;

    if(service.proto == TGX_TCP)
    {
        err = net_tcp_open(&listener_fd, service.port); 
        if(err != TGX_OK)
        {
            return err;
        }
    }
    else if(service.proto == TGX_UDP)
    {
        err = net_udp_open(&listener_fd, service.port);
        if(err != TGX_OK)
        {
            return err;
        }
    }

    struct tgx_file *file = file_alloc(listener_fd, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR, service.proto);
    if(file == NULL)
    {
        close(listener_fd);
        return TGX_ALLOC_ERROR;
    }

    file->sock.is_listen = true;
    file->sock.pair_fd = -1;

    if(eventloop_addevent(eventloop, file) != TGX_OK)
    {
        close(listener_fd);
        free(file);
        return TGX_ADD_EVENT_ERROR;
    }

    return TGX_OK;
}

/**
 * TGX_PIPE : Level-Trigger - 그자리에서 라우팅 테이블 업데이트
 * TCP or UDP : Edge Trigger => 외부스레드에서 처리예정
*/
void tgx_worker_poll(struct tgx_server *server, struct tgx_eventloop *eventloop, int index)
{
    struct tgx_service service = server->service[index];
    while(true)
    {
        int retval = eventloop_poll(eventloop);
        if(retval <= 0)
        {
            continue;
        }

        for(int i=0; i<retval; i++)
        {
            struct tgx_file *file = eventloop->fired[i];
            if((file->fire_event & EPOLLERR)
            || (file->fire_event & EPOLLRDHUP)
            || (file->fire_event & EPOLLHUP))
            {
                /* DELETE FILE */
                if(eventloop_delevent(eventloop, file) != TGX_OK)
                {
                    continue;
                }
                printf("DELETE EVENT\n");
            }

            /* 읽기 이벤트가 없다면 */
            if(!(file->fire_event & EPOLLIN))
            {
                /* DELETE FILE */
                continue;
            }
            
            switch (file->type)
            {
                case TGX_PIPE:
                {
                    break;
                }

                case TGX_TCP:
                {
                    /**
                     * 상대 프록시로부터 데이터가 온다면 서비스주소:포워드_포트로 패킷 릴레이
                     * 이 모든 과정은 스레드풀 이용
                    */
                    if(file->sock.is_listen)
                    {
                        /**
                         * process accept and connect 
                        */
                        if(tgx_worker_tcp_process(eventloop, file, service) != TGX_OK)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        /**
                         * 스레드풀 이용해서 read send 진행
                        */
                       thpool_add_work(eventloop->thpool, tgx_worker_reverse_proxy, (void*)file);
                    }
                    
                    break;
                }
                    
                case TGX_UDP:
                    break;
                
                default:
                    break;
            }
        }

        free(eventloop->fired);
    }
}

tgx_err_t tgx_worker_tcp_process(struct tgx_eventloop *eventloop, 
                            struct tgx_file *listener, 
                            struct tgx_service service)
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

    if(net_tcp_connect(&connect_fd, service.host, service.forward_port) != TGX_OK)
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

    accept_file->sock.pair_fd = connect_file->fd;
    connect_file->sock.pair_fd = accept_file->fd;

    printf("accept fd : %d\n", accept_fd);
    printf("connect fd : %d\n", connect_fd);

    return TGX_OK;
}

/* read하고 send */
/**
 * EPOLLET이기때문에 읽거나 보낼게 없을 때 까지 루프 지속
 * 어차피 스레드에서 진행할건데 NON_BLOCK으로 안해도될듯 
*/
void tgx_worker_reverse_proxy(void *arg)
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
            else
            {
                break;
            }
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
        }
        else
        {
            break;
        }
    }

    while(true)
    {
        int ret = write(file->sock.pair_fd, buffer, total_size);
        if(ret < 0)
        {
            if(errno == EAGAIN)
            {
                continue;
            }

            break;
        }

        break;
    }

    return;
}

void set_handler()
{

}