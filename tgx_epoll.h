#ifndef __TGX_EVENT_H_
#define __TGX_EVENT_H_

#include "common.h"
#include "sds.h"
#include "tgx_net.h"
#include "tgx_errno.h"
#include "list.h"
#include "thpool.h"

#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>

#define MAX_EPOLL_SIZE 1024
#define INFINITE -1
#define MAX_THREAD_POOL 10



/**
 * PIPE Line 포함해서 사용가능
 * read or write 
 * 
 * list_head에서 head는 데이터 따로 저장안하는듯 
 */
struct tgx_file
{
    int fd;
    int mask;
    int type; /* TCP UDP PIPE fd */
    int fire_event; /* 활성화된 이벤트 */
    int pair_fd;
    /**
     * pipe 파일에는 할당 X 
    */
    struct tgx_sock
    {
        bool is_listen; /* listener fd인지 확인 */
        int pair_fd;    /* 종단의 파일디스크립터 */
    } sock;
    // read
    // write
    struct list_head list;
};

struct tgx_eventloop
{
    int epfd;
    int maxsize; /* epoll 이벤트 확인 수 */
    int timeout;
    struct list_head events;  /* 관리하는 전체 파일들 */ 
    struct tgx_file **fired;   /* polling을 통해 활성화된 파일들 */
    threadpool thpool;      /* 스레드 풀 */
};

struct thr_worker_arg
{
    struct tgx_service service;
    int fd;
};

tgx_err_t do_pipe(struct tgx_server *server, struct tgx_eventloop *eventloop, int index);
tgx_err_t do_bind(struct tgx_server *server, struct tgx_eventloop *eventloop, int index);
struct tgx_file *file_alloc(int fd, int mask, int type);
struct tgx_eventloop *eventloop_alloc(int timeout);
tgx_err_t eventloop_addevent(struct tgx_eventloop *eventloop, struct tgx_file *file);
tgx_err_t eventloop_delevent(struct tgx_eventloop *eventloop, struct tgx_file *file);
int eventloop_poll(struct tgx_eventloop *eventloop);
void set_handler();

#endif