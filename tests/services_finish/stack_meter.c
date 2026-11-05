#ifndef INK_TEST_VTK_SOURCE
#define INK_TEST_VTK_SOURCE "../../simulator/kernel/vtk/vtk.c"
#endif
#include INK_TEST_VTK_SOURCE

int main(void) {
    _Alignas(uintptr_t) uint8_t stack[128];
    vtk_task_t task = {.own_lo = stack, .entry = stack + sizeof stack};
    int failed = 0;
    /* The same live stack word may start with any number of pattern-valued address bytes. */
    for (size_t prefix = 0; prefix < sizeof(uintptr_t); prefix++) {
        memset(stack, STACK_PATTERN, sizeof stack);
        memset(stack + 64 + prefix, 0, sizeof stack - 64 - prefix);
        size_t used = own_stack_used(&task);
        if (used != 64) {
            fprintf(stderr, "prefix=%zu used=%zu expected=64\n", prefix, used);
            failed++;
        }
    }
    memset(stack, STACK_PATTERN, sizeof stack);
    if (own_stack_used(&task) != 0) failed++;
    task.entry -= 3;
    if (own_stack_used(&task) != 0) failed++;
    task.entry += 3;
    memset(stack, 0, sizeof stack);
    if (own_stack_used(&task) != sizeof stack) failed++;
    printf("stack meter: %d failures\n", failed);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
