#include <kern/handle.h>

uint64_t handle_counter;

uint64_t
handle_alloc()
{
    return handle_counter++;
}
