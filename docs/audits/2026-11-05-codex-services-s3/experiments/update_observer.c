/* Unmodified product translation unit; read-only observation of its download. */
#include "../../../../system/services/update/update_service.c"
int audit_waiting_chunk(void) { return __atomic_load_n(&U.dl_waiting, __ATOMIC_ACQUIRE) == 1 && U.job > 0; }
int audit_download_task(void) { return U.task; }
