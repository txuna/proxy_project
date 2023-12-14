#include "tgx_net.h"

// serAdd.sin_addr.s_addr = inet_addr("127.0.0.1");

static int __net_tcp_open(int port)
{
    int flags;
    int on =1;
    struct sockaddr_in addr; 
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port); 

    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        goto ret;
    }

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        goto err;
    }

    if((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
        goto err;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        goto err;
    }

    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        goto err;
    }

    if(listen(fd, BACKLOG) < 0)
    {
        goto err;
    }

ret: 
    return fd;

err:
    close(fd);
    return -1;
}

tgx_err_t net_tcp_open(int* fd, int port)
{
    *fd = __net_tcp_open(port);

    if(*fd < 0)
    {
        return TGX_SOCK_ERROR;
    }

    return TGX_OK;
}

static int __net_udp_open(int port)
{
    int flags;
    int on =1;
    struct sockaddr_in addr; 
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port); 

    int fd = socket(PF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
    {
        goto ret;
    }

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        goto err;
    }

    if((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
        goto err;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        goto err;
    }

    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        goto err;
    }

ret: 
    return fd;

err:
    close(fd);
    return -1;
}

tgx_err_t net_udp_open(int* fd, int port)
{
    *fd = __net_udp_open(port);

    if(*fd < 0)
    {
        return TGX_SOCK_ERROR;
    }

    return TGX_OK;
}

static int __net_tcp_connect()
{

}

tgx_err_t net_tcp_connect()
{

}

static int __net_tcp_accept()
{

}

tgx_err_t net_tcp_accept()
{

}

static int __net_tcp_read()
{

}

tgx_err_t net_tcp_read()
{

}

static int __net_tcp_send()
{

}

tgx_err_t net_tcp_send()
{

}

static int __net_udp_read()
{

}

tgx_err_t net_udp_read()
{

}

static int __net_udp_send()
{

}

tgx_err_t net_udp_send()
{

}

static int __pipe_read()
{

}

tgx_err_t pipe_read()
{

}

static int __pipe_send()
{

}

tgx_err_t pipe_send()
{

}