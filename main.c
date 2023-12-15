#include "main.h"


int main(int argc, char **argv)
{
    struct tgx_server *server = calloc(1, sizeof(struct tgx_server));
    if(server == NULL)
    {
        return 1;
    }

    if(parse_cgf(server) != TGX_OK)
    {
        goto clean;
    }

    if(init_server(server) != TGX_OK)
    {
        goto clean;
    }

    if(do_proxy_eventloop(server) != TGX_OK)
    {
        goto kill_process;
    }

clean:
    /* wait process */
    cleanup_process(server);
    free_server(server);
    return 0;

kill_process:
    kill_process(server);
    goto clean;
}


void kill_process(struct tgx_server *server)
{
    if(server == NULL) return;
    if(server->process_list == NULL) return;

    /* Parent Process 실패시 자식프로세스 강제 정리 */
    for(int i=0; i<server->service_len; i++)
    {
        kill(server->process_list[i], SIGTERM);
    }
}

/**
 * 프록시 서버 초기화 
*/
tgx_err_t init_server(struct tgx_server *server)
{
    /* 서비스 수 만큼 process id 배열 생성 */
    server->process_list = (pid_t*)calloc(server->service_len, sizeof(pid_t));
    if(server->process_list == NULL)
    {
        return TGX_ALLOC_ERROR;
    }

    server->pipes = (int**)calloc(server->service_len, sizeof(int*)); 
    if(server->pipes == NULL)
    {
        return TGX_ALLOC_ERROR;
    }

    for(int i=0; i<server->service_len; i++)
    {
        server->pipes[i] = (int*)calloc(2, sizeof(int));
        if(server->pipes[i] == NULL)
        {
            return TGX_ALLOC_ERROR;
        }

        if(pipe(server->pipes[i]) == -1)
        {
            return TGX_PIPE_ERROR;
        }
    }

    /* 서비스마다 프로세스 생성하고 epoll instance 생성 */
    for(int i=0; i<server->service_len; i++)
    {
        pid_t pid = fork();
        if(pid < 0)
        {
            fprintf(stderr, "Failed Create Process - Service Name : %s\n", server->service[i].name);
            break;
        }

        else if(pid == 0)
        {
            /**
             * 해당 프로세스가 맡게될 서비스와 라우팅 테이블 전송 
            */
            tgx_worker_process(server, i);
            exit(0);
        }
    }

    return TGX_OK;
}


tgx_err_t do_proxy_eventloop(struct tgx_server *server)
{
    struct tgx_eventloop *eventloop = eventloop_alloc(INFINITE);
    if(eventloop == NULL)
    {
        return TGX_ALLOC_ERROR;
    }

    /* 파이프 세팅 */
    if(do_proxy_pipe(server, eventloop) != TGX_OK)
    {
        return TGX_PIPE_ERROR;
    }

    if(do_proxy_bind(server, eventloop) != TGX_OK)
    {
        return TGX_SOCK_ERROR;
    }

    /* 스레드 풀 생성 */
    eventloop->thpool = thpool_init(MAX_THREAD_POOL);

    /* 메인 프록시 폴링 시작 */
    do_porxy_poll(server, eventloop);
    
    /* 서비스 종료 */
    free_eventloop(eventloop);
    return TGX_OK;
}

/**
 * Main Process의 역할은 자신의 클라우드에서 서비스중인 서비스가 상대 클라우드에 커넥션을 진행할 때 
 * 프록시 느낌 - 이때는 라우팅 테이블 필요
 * 파이프라인이 필요없지만 추후 대비해서 남겨놓기 
*/
void do_porxy_poll(struct tgx_server *server, struct tgx_eventloop *eventloop)
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
            || (file->fire_event & EPOLLRDHUP)
            || (file->fire_event & EPOLLHUP))
            {
                printf("[MASTER]DELETE EVENT : %d\n", file->fd);
                /* DELETE FILE */
                if(eventloop_delevent(eventloop, file) != TGX_OK)
                {
                    continue;
                }
            }

            /* 특정 클라우드 라우팅 테이블이 도착하면 기존꺼 덮기 */
            switch (file->type)
            {
            case TGX_TCP:
            {
                if(file->sock.is_listen)
                {
                    if(do_proxy_tcp_process(server, eventloop, file) != TGX_OK)
                    {
                        continue;
                    }
                }
                else
                {
                    thpool_add_work(eventloop->thpool, do_proxy_network_process, (void*)file);
                }
                break;
            }
            
            default:
                break;
            }
        }

        free(eventloop->fired);
    }
}

struct tgx_route_table *find_route(struct tgx_server *server, int port)
{
    struct tgx_route_table *rt; 
    list_for_each_entry(rt, &server->route_table, list)
    {
        if(rt->port == port)
        {
            return rt;
        }
    }

    return NULL;
}

/**
 * accept and connect
*/
tgx_err_t do_proxy_tcp_process(struct tgx_server *server, struct tgx_eventloop *eventloop, struct tgx_file *listener)
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

    struct tgx_route_table *rt = find_route(server, listener->sock.v.port);
    if(rt == NULL)
    {
        eventloop_delevent(eventloop, accept_file);
    }

    if(net_tcp_connect(&connect_fd, rt->host, rt->port) != TGX_OK)
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

void do_proxy_network_process(void *arg)
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
        int ret = write(file->sock.v.pair_fd, buffer, total_size);
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

/**
 * 라우팅 테이블만큼 바인딩 하기
*/
tgx_err_t do_proxy_bind(struct tgx_server *server, struct tgx_eventloop *eventloop)
{
    tgx_err_t err; 
    struct tgx_route_table *rt;
    list_for_each_entry(rt, &server->route_table, list)
    {
        int bind_fd;
        err = net_tcp_open(&bind_fd, rt->port);
        if(err != TGX_OK)
        {
            return TGX_SOCK_ERROR;
        }

        struct tgx_file *file = file_alloc(bind_fd, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR, TGX_TCP);
        if(file == NULL)
        {
            close(bind_fd);
            return TGX_ALLOC_ERROR;
        }

        if(eventloop_addevent(eventloop, file) != TGX_OK)
        {
            close(bind_fd);
            free(file);
            return TGX_ADD_EVENT_ERROR;
        }

        file->sock.is_listen = true;
        file->sock.v.port = rt->port;
    }

    return TGX_OK;
}

tgx_err_t do_proxy_pipe(struct tgx_server *server, struct tgx_eventloop *eventloop)
{
    for(int i=0; i<server->service_len; i++)
    {
        int *pipe = server->pipes[i];
        close(pipe[out]);

        /**
         * 해당 디스크립터는 이벤트 알림을 받지 않고 직접 루프돌려서 라우팅 테이블 전송할 것이기에 
         * 필요없는 플래그인 EPOLLIN으로 세팅 - 해당 디스크립터는 write만 가능
        */
        struct tgx_file *file = file_alloc(pipe[in], EPOLLIN, TGX_PIPE);
        if(file == NULL)
        {
            return TGX_ALLOC_ERROR;
        }

        if(eventloop_addevent(eventloop, file) != TGX_OK)
        {
            return TGX_ADD_EVENT_ERROR;
        }
    }

    return TGX_OK;
}


void cleanup_process(struct tgx_server *server)
{
    if(server == NULL)
    {
        return;
    }

    while(true)
    {
        int status;
        pid_t done = wait(&status);
        if(done == -1)
        {
            if(errno == ECHILD)
            {
                break;
            }
        }
        else
        {
            if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                printf("Failed PID : %d-%d\n", done, status);
                break;
            }
        }
    }
} 


void free_eventloop(struct tgx_eventloop *eventloop)
{
    struct tgx_file *file, *tmp;

    if(eventloop == NULL)
    {
        return;
    }

    /* 관리하는 파일 목록 제거 */
    list_for_each_entry_safe(file, tmp, &eventloop->events, list)
    {
        list_del(&file->list);
        close(file->fd);
        free(file);
    }

    /* 스레드 풀 제거 */
    thpool_wait(eventloop->thpool);
    thpool_destroy(eventloop->thpool);

    close(eventloop->epfd);
    free(eventloop);
    return;
}


void free_server(struct tgx_server *server)
{
    struct tgx_route_table *rt, *tmp;
    if(server == NULL)
    {
        return;
    }

    /* 클라우드 리스트 해제 */
    sdsfree(server->service_name);
    if(server->cloud != NULL)
    {
        for(int i=0; i<server->cloud_len; i++)
        {
            sdsfree(server->cloud[i].name);
            sdsfree(server->cloud[i].host);
        }

        free(server->cloud);
    }

    /* 서비스 목록 해제 */
    if(server->service != NULL)
    {
        for(int i=0; i<server->service_len; i++)
        {
            sdsfree(server->service[i].name);
            sdsfree(server->service[i].host);
        }

        free(server->service);
    }

    /* 통신 파이프라인 닫기 */
    if(server->pipes != NULL)
    {
        for(int i=0; i<server->service_len; i++)
        {
            if(server->pipes[i] != NULL)
            {
                close(server->pipes[i][0]);
                close(server->pipes[i][1]);
                free(server->pipes[i]);
            }
        }

        free(server->pipes);
    }

    /* 프로세스 리스트 해제 */
    if(server->process_list != NULL)
    {
        free(server->process_list);
    }

    /* 라우팅 테이블 해제 */
    list_for_each_entry_safe(rt, tmp, &server->route_table, list)
    {
        list_del(&rt->list);
        sdsfree(rt->host);
        free(rt);
    }

    free(server);
    return;
}


tgx_err_t parse_cgf(struct tgx_server *server)
{
    config_t cfg; 
    config_setting_t *setting;
    const char *sname;
    
    INIT_LIST_HEAD(&server->route_table);
    config_init(&cfg);

    if(!config_read_file(&cfg, "config.cfg"))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), 
                                        config_error_line(&cfg),
                                        config_error_text(&cfg));
        
        config_destroy(&cfg);
        return TGX_PARSE_ERROR;
    }

    if(!config_lookup_string(&cfg, "service_name", &sname))
    {
        fprintf(stderr, "Cannot Found Service Name\n");
        config_destroy(&cfg);
        return TGX_PARSE_ERROR;
    }

    if(!config_lookup_int(&cfg, "bind_port", &server->bind_port))
    {
        fprintf(stderr, "Cannot Found Bind Port\n");
        config_destroy(&cfg);
        return TGX_PARSE_ERROR;
    }

    server->service_name = sdsnew(sname);

    setting = config_lookup(&cfg, "application.clouds");
    if(setting != NULL)
    {
        int count = config_setting_length(setting);

        server->cloud_len = count;
        server->cloud = (struct tgx_cloud *)calloc(count, sizeof(struct tgx_cloud));
        if(server->cloud == NULL)
        {
            config_destroy(&cfg);
            return TGX_ALLOC_ERROR;
        }

        for(int i=0; i<count; i++)
        {
            config_setting_t *obj = config_setting_get_elem(setting, i);
            const char *name, *host; 
            int port;

            if(!(config_setting_lookup_string(obj, "name", &name)
                && config_setting_lookup_string(obj, "host", &host)
                && config_setting_lookup_int(obj, "port", &port)))
            {
                continue;
            }

            server->cloud[i].name = sdsnew(name);
            server->cloud[i].host = sdsnew(host);
            server->cloud[i].port = port;

            config_setting_t *list_setting = config_setting_lookup(obj, "list");
            if(!list_setting)
            {
                continue;
            }

            /* 라우팅 테이블 추가 */
            int llen = config_setting_length(list_setting);
            for(int j=0; j<llen; j++)
            {
                config_setting_t *tmp = config_setting_get_elem(list_setting, j);
                int value = config_setting_get_int(tmp);
                struct tgx_route_table *rt = calloc(1, sizeof(struct tgx_route_table));
                if(rt == NULL)
                {
                    continue;
                } 
                rt->host = sdsnew(host); 
                rt->port = value;
                //inet_pton(AF_INET, host, &rt->addr.sin_addr);
                //rt->addr.sin_port = htons(value);

                list_add_tail(&rt->list, &server->route_table);
            }
        }
        
    }

    setting = config_lookup(&cfg, "application.services");
    if(setting != NULL)
    {
        int count = config_setting_length(setting);
        
        server->service_len = count;
        server->service = (struct tgx_service *)calloc(count, sizeof(struct tgx_service));
        if(server->service == NULL)
        {
            config_destroy(&cfg);
            return TGX_ALLOC_ERROR;
        }

        for(int i=0; i<count; i++)
        {
            config_setting_t *obj = config_setting_get_elem(setting, i);
            const char *name, *host; 
            int port, forward_port;
            proto_t proto;

            if(!(config_setting_lookup_string(obj, "name", &name)
                && config_setting_lookup_string(obj, "host", &host)
                && config_setting_lookup_int(obj, "port", &port)
                && config_setting_lookup_int(obj, "forward_port", &forward_port)
                && config_setting_lookup_int(obj, "proto", &proto)))
            {
                continue;
            }

            server->service[i].name = sdsnew(name);
            server->service[i].host = sdsnew(host);
            server->service[i].port = port;
            server->service[i].proto = proto;
            server->service[i].forward_port = forward_port;
        }
    }

    config_destroy(&cfg);
    return TGX_OK;
}