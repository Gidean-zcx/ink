/* Local member and credential surfaces. UI task only. */
#ifndef INK_MEMBER_PAGES_H
#define INK_MEMBER_PAGES_H
int ink_member_pages_init(void);
void ink_shell_member_switch_open(void);
void ink_shell_member_pin_open(const char *id);
void ink_shell_member_gate_done(void);
#endif
