/* Member-scoped backup/restore capabilities. Work runs on exec.bg; no UI or host dependencies. */
#ifndef INK_SYS_BACKUP_H
#define INK_SYS_BACKUP_H

#ifdef __cplusplus
extern "C" {
#endif

int ink_backup_init(void);
/* After members, registered collections and the hub are ready; resumes durable work for the current member. */
void ink_backup_start(void);

#ifdef __cplusplus
}
#endif
#endif
