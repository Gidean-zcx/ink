/* C++ clients must see the same C ABI as the carrier implementation. */
#include "graft.h"
extern "C" bool ink_graft_current(void);
