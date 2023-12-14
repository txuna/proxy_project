#ifndef __TGX_ERRNO_H_
#define __TGX_ERRNO_H_

#include <errno.h>

typedef int tgx_err_t;

#define TGX_OK 0
#define TGX_NORMAL_ERROR 1
#define TGX_ALLOC_ERROR 2
#define TGX_PARSE_ERROR 3
#define TGX_ADD_EVENT_ERROR 4
#define TGX_DEL_EVENT_ERROR 5
#define TGX_PIPE_ERROR 6
#define TGX_SOCK_ERROR 7

#endif 