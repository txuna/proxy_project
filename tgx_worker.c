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

    struct tgx_file *file = file_alloc(listener_fd, EPOLLIN, service.proto);
    if(file == NULL)
    {
        close(listener_fd);
        return TGX_ALLOC_ERROR;
    }

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
            || (file->fire_event & EPOLLRDHUP))
            {
                /* DELETE FILE */
                if(eventloop_delevent(eventloop, file) != TGX_OK)
                {
                    continue;
                }
            }
            
            switch (file->type)
            {
            case TGX_PIPE:
            {
                /**
                 * 라우팅 테이블 덮기(업데이트)
                */
                break;
            }

            case TGX_TCP:
            {
                /**
                 * 해당 서비스포트를 라우팅 테이블에서 찾고 해당 주소로 소켓 만들고 connect
                 * 그리고 해당 소켓은 종단에서 끊을 때까지 이벤트 보관
                 * 이벤트가 도착하면 페어 파일에게 write 
                 * 이때 페어파일은? 어떻게 알지? 따로 저장? 
                 * 
                 * 또한 이 모든 작업은 스레드 풀에서 처리?
                 * 그럼 eventloop랑 server lock걸어야 하나?
                 * 아니면 그냥 스레드가 알아서 위 모든 과정을 처리? - 요청 하나당 스레드? 그럼 따로 넣고 빼고 할 필요없이
                 * 스레드 넘겨줄 때 그냥 라우팅 테이블만 찾아서 넣어주면 될듯
                */
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

void set_handler()
{

}