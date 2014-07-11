
#ifndef __PATH_ADMIN_H__
#define __PATH_ADMIN_H__

#include <tasks/path_worker.h>
#define PATH_ADMIN_NAME "P_ADMIN"

typedef enum {
    PA_GET_PATH,
    PA_REQ_WORK,
    PA_REQ_COUNT
} pa_req_type;

typedef struct {
    pa_req_type  type;
    path_request req;
} pa_request;

void __attribute__((noreturn)) path_admin(void);

#endif

