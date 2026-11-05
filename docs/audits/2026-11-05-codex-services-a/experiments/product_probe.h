#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/hal_storage.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/storage.h"
#include "ink/sys/task.h"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
#include "../../../../tests/integration/whole_product.h"
