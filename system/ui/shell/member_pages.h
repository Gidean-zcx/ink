/* Local member and credential surfaces. UI task only. */
#ifndef INK_MEMBER_PAGES_H
#define INK_MEMBER_PAGES_H
#include <stdint.h>
#define INK_PARENT_PIN_MISSING "家里还没有大人设密码，请大人先在设置里设一个"
int ink_member_pages_init(void);
void ink_shell_member_parent_open(uint32_t token, const char *why);
void ink_shell_member_switch_open(void);
void ink_shell_member_pin_open(const char *id);
void ink_shell_member_gate_done(void);
#endif
