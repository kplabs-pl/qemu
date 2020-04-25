#include "qemu/osdep.h"
#include "qemu/main-loop.h"

void qemu_set_fd_handler(fd_handle_type fd,
                         IOHandler *fd_read,
                         IOHandler *fd_write,
                         void *opaque)
{
    abort();
}
