#ifndef EDI_H
#define EDI_H

#include "qemu/osdep.h"
#include "hw/qdev-core.h"

typedef enum
{
    edi_connection_state_disconnected = 0,
    edi_connection_state_connected = 1
} edi_connection_state;

typedef enum
{
    edi_mode_req = 0,
    edi_mode_rep = 1,
    edi_mode_pair = 2,
    edi_mode_pull = 3,
    edi_mode_push = 4,
    edi_mode_pub = 5,
    edi_mode_sub = 6,
    edi_mode_ensure_size = 0xffffffff
} edi_communication_mode;

// TODO: fix
typedef int fd_handle_type;

typedef struct kp_edi_message_chunk
{
    QSIMPLEQ_ENTRY(kp_edi_message_chunk) list_entry;
    void* message;
    size_t size;
} kp_edi_message_chunk;

typedef struct
{
    QSIMPLEQ_HEAD(msg_chunk_list, kp_edi_message_chunk) head;
    size_t size;
} kp_edi_chunk_list;

typedef struct KPEDIState {
    DeviceState parent_obj;

    uint64_t addr;
    qemu_irq irq;

    struct {
        uint32_t command;
        uint32_t pointer;
        uint32_t size;
        uint32_t interrupt;
        uint32_t id;
    } registers;

    edi_connection_state connection_state;
    edi_communication_mode communication_mode;

    int socket;
    fd_handle_type socket_read_handle;
    fd_handle_type socket_write_handle;

    kp_edi_chunk_list send_list;
    kp_edi_chunk_list receive_list;

    void (*trigger_irq)(struct KPEDIState* self);
} KPEDIState;

#define TYPE_KP_EDI "kp-edi"
#define KP_EDI(obj) OBJECT_CHECK(KPEDIState, (obj), \
                                         TYPE_KP_EDI)

#define KP_EDI_ADDRESS_SPACE_SIZE 0x20

#endif