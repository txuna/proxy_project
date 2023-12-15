#ifndef __TGX_WORKER_H_
#define __TGX_WORKER_H_

#include "common.h"
#include "tgx_epoll.h"
#include "tgx_net.h"
#include "tgx_errno.h"
#include "main.h"
#include "thpool.h"

void tgx_worker_process(struct tgx_server *server, int index);
void tgx_worker_poll(struct tgx_server *server, struct tgx_eventloop *eventloop, int index);
tgx_err_t tgx_worker_tcp_process(struct tgx_eventloop *eventloop, struct tgx_file *listener, struct tgx_service service);
void tgx_worker_reverse_proxy(void *arg);

#endif 