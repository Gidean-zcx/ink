#include "ink/sys/svc.h"
extern void audit_task_unlock(void);
#define ink_svc_unlock audit_task_unlock
#include "../../../../system/services/res/task.c"
#undef ink_svc_unlock
