/* Compile-only layout check. RV32 layout is separate from host pool accounting. */
#include "ink/sys/args.h"
typedef struct { char from[24]; char text[200]; } audit_msg_t;
_Static_assert(sizeof(void *)==4,"RV32 expected");
_Static_assert(sizeof(ink_args_t)==584,"argument layout changed");
_Static_assert(sizeof(audit_msg_t)==224,"message layout changed");
_Static_assert(2*sizeof(ink_args_t)+sizeof(audit_msg_t)==1392,"scratch footprint changed");
