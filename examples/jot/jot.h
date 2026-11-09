#ifndef INK_EXAMPLE_JOT_H
#define INK_EXAMPLE_JOT_H

/* Boot registration; this example is linked only into its test executable. */
int ink_jot_init(void);
/* Before opening a stopped app, load this member's collection outside UI. */
int ink_jot_prepare(void);

#endif
