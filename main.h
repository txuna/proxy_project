#ifndef __MAIN_H_
#define __MAIN_H_

#include "common.h"
#include "tgx_errno.h"
#include <libconfig.h>
#include <stdlib.h>
#include <unistd.h>

#include "sds.h"
#include "tgx_net.h"
#include "tgx_worker.h"
#include "tgx_epoll.h"
#include "list.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void free_eventloop(struct tgx_eventloop *eventloop);
void free_server(struct tgx_server *server);
tgx_err_t parse_cgf(struct tgx_server *server);

tgx_err_t init_server(struct tgx_server *server);

void cleanup_process(struct tgx_server *server);
void kill_process(struct tgx_server *server);
tgx_err_t do_proxy_eventloop(struct tgx_server *server);
tgx_err_t do_proxy_pipe(struct tgx_server *server, struct tgx_eventloop *eventloop);
tgx_err_t do_proxy_bind(struct tgx_server *server, struct tgx_eventloop *eventloop);
void do_porxy_poll(struct tgx_server *server, struct tgx_eventloop *eventloop);

struct tgx_route_table *find_route(struct tgx_server *server, int port);
tgx_err_t do_proxy_tcp_process(struct tgx_server *server, struct tgx_eventloop *eventloop, struct tgx_file *listener);
void do_proxy_network_process(void *arg);


#endif