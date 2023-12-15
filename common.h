#ifndef __COMMON_H_
#define __COMMON_H_

#include <stdint.h>
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

#include "sds.h"
#include "thpool.h"
#include "list.h"

typedef int socket_t;
typedef int type_t;

typedef int proto_t;

#define TGX_PIPE 0
#define TGX_TCP 1
#define TGX_UDP 2

#define true 1
#define false 0

#define in 1        /* pipe의 경우 write(pipe[1])*/
#define out 0       /* pipe의 경우 read(pipe[0])*/

typedef uint8_t bool;

struct tgx_cloud
{
    sds name;
    sds host; 
    int port;
};

struct tgx_service
{
    sds name;
    sds host; 
    int port;
    int forward_port;
    proto_t proto;
};

/**
 * 다른 클라우드가 관리하는 서비스 포트들
 * 패킷 릴레이의 경우 목적지 포트를 기반으로 프록시 서버의 주소 찾기
*/
struct tgx_route_table
{
    sds host; 
    int port;
    struct list_head list;
};

struct tgx_server
{
    /* 추후 mutex 검토 */
    sds service_name;
    int cloud_len; 
    int service_len;
    struct tgx_cloud *cloud; 
    struct tgx_service *service;
    struct list_head route_table;    /* 라우팅 테이블 */
    int bind_port;
    pid_t *process_list;        /* 프로세스 리스트 - server len */
    int **pipes;                /* service len */
};


#endif