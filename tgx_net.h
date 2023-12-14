#ifndef __NET_H_
#define __NET_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/ip.h> 

#include "tgx_errno.h"
#include "common.h"

#define BACKLOG 1024

tgx_err_t net_tcp_open(int* fd, int port);
tgx_err_t net_udp_open(int* fd, int port);

#endif