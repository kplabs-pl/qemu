#include "edi-commands.h"
#include "edi-list.h"
#include "trace/trace-hw_kp.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "qemu/log.h"
#include "block/aio.h"
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

static inline size_t min_size_t(size_t left, size_t right)
{
    return left < right ? left : right;
}

static const size_t SendQueueLengthLimit = 128;

static void write_handler(void* device);
static void read_handler(void* device);

static void register_write_handle(KPEDIState* device)
{
    aio_set_fd_handler(
        qemu_get_current_aio_context(),
        device->socket_write_handle,
        true,
        NULL,
        write_handler,
        NULL,
        NULL,
        device
        );
}

static void register_read_handle(KPEDIState* device)
{
    aio_set_fd_handler(
        qemu_get_current_aio_context(),
        device->socket_read_handle,
        true,
        read_handler,
        NULL,
        NULL,
        NULL,
        device
        );
}

static void unregister_write_handle(KPEDIState* device)
{
    aio_set_fd_handler(qemu_get_current_aio_context(), device->socket_write_handle, true, NULL, NULL, NULL, NULL, NULL);
}

static void unregister_read_handle(KPEDIState* device)
{
    aio_set_fd_handler(qemu_get_current_aio_context(), device->socket_read_handle, true, NULL, NULL, NULL, NULL, NULL);
}

static void write_handler(void* device)
{
    KPEDIState* state = KP_EDI((Object*)device);
    trace_kp_edi_ready_to_send_more(state->name);

    int status = 0;
    size_t cx = 0;
    for(; status != -1 && state->send_list.size > 0; ++cx)
    {
        kp_edi_message_chunk* chunk = kp_edi_chunk_list_pop_front(&state->send_list);
        status = nn_send(state->socket, &chunk->message, NN_MSG, NN_DONTWAIT);
        if(status != -1)
        {
            trace_kp_edi_error(state->name, "Send failed");
            chunk->message = NULL;
            kp_edi_destroy_message_chunk(chunk);
        }
        else
        {
            kp_edi_chunk_list_push_front(&state->send_list, chunk);
        }
    }

    trace_kp_edi_message_sent(state->name, cx, state->send_list.size);

    if(state->send_list.size == 0)
    {
        unregister_write_handle(state);
    }
}

static void read_handler(void* device)
{
    KPEDIState* state = KP_EDI((Object*)device);
    trace_kp_edi_message_incoming(state->name);
    
    kp_edi_message_chunk* chunk = kp_edi_create_message_chunk();
    const int result = nn_recv(state->socket, &chunk->message, NN_MSG, NN_DONTWAIT);
    if(result == -1)
    {
        if(nn_errno() != EAGAIN)
        {
            trace_kp_edi_error_reason(state->name, "unable to receive message", nn_strerror(nn_errno()));
        }

        kp_edi_destroy_message_chunk(chunk);
        return;
    }

    chunk->size = result;
    kp_edi_chunk_list_push_back(&state->receive_list, chunk);
    trace_kp_edi_message_received(state->name, chunk->size, state->receive_list.size);

    state->trigger_irq(state);
}

static bool get_socket_write_handle(KPEDIState* device, int socket, fd_handle_type* handle)
{
    size_t size = sizeof(*handle);
    const bool status = nn_getsockopt(socket, NN_SOL_SOCKET, NN_SNDFD, handle, &size) == 0;
    if(!status)
    {
        trace_kp_edi_error_reason(device->name, "Unable to access socket handle for writing", nn_strerror(nn_errno()));
    }

    return status;
}

static bool get_socket_read_handle(KPEDIState* device, int socket, fd_handle_type* handle)
{
    size_t size = sizeof(*handle);
    const bool status = nn_getsockopt(socket, NN_SOL_SOCKET, NN_RCVFD, handle, &size) == 0;
    if(!status)
    {
        trace_kp_edi_error_reason(device->name, "Unable to access socket handle for reading", nn_strerror(nn_errno()));
    }

    return status;
}

typedef bool (*socket_setup_function)(KPEDIState* device, int socket, fd_handle_type* handle);
typedef void (*register_socket)(KPEDIState* device);

static const struct edi_communication_pair
{
    int nanomsg_type;
    const char* name;
    socket_setup_function read_setup;
    socket_setup_function write_setup;
    register_socket read_register;
    register_socket write_register;
    register_socket read_unregister;
    register_socket write_unregister;
} communication_map[] =
{
    {NN_REQ, "req", get_socket_read_handle, get_socket_write_handle, register_read_handle, register_write_handle, unregister_read_handle, unregister_write_handle},
    {NN_REP, "rep", get_socket_read_handle, get_socket_write_handle, register_read_handle, register_write_handle, unregister_read_handle, unregister_write_handle},
    {NN_PAIR, "pair", get_socket_read_handle, get_socket_write_handle, register_read_handle, register_write_handle, unregister_read_handle, unregister_write_handle},
    {NN_PULL, "pull", get_socket_read_handle, NULL, register_read_handle, NULL, unregister_read_handle, NULL},
    {NN_PUSH, "push", NULL, get_socket_write_handle, NULL, register_write_handle, NULL, unregister_write_handle},
    {NN_PUB, "pub", NULL, get_socket_write_handle, NULL, register_write_handle, NULL, unregister_write_handle},
    {NN_SUB, "sub", get_socket_read_handle, NULL, register_read_handle, NULL, unregister_read_handle, NULL},
};

#define COMMUNICATION_MAP_SIZE (sizeof(communication_map) / sizeof(*communication_map))

static const struct edi_communication_pair* find_communication_mode(edi_communication_mode mode)
{
    static const struct edi_communication_pair invalid = {-1, "invalid", NULL, NULL, NULL, NULL, NULL, NULL};
    if((int)mode < COMMUNICATION_MAP_SIZE)
    {
        return communication_map + (int)mode;
    }
    else
    {
        return &invalid;
    }
}

static void drop_outgoing_message(KPEDIState* device, void* message)
{
    nn_freemsg(message);
    trace_kp_edi_message_dropped(device->name, device->send_list.size, nn_strerror(nn_errno()));
}

static bool enquene_outgoing_message(KPEDIState* device, void* message, size_t size)
{
    const struct edi_communication_pair* mode = find_communication_mode(device->communication_mode);
    const bool status = device->send_list.size < SendQueueLengthLimit && mode->write_register != NULL;
    if(status)
    {
        if(kp_edi_chunk_list_empty(&device->send_list))
        {
            mode->write_register(device);
        }

        kp_edi_message_chunk* chunk = kp_edi_create_message_chunk();
        chunk->message = message;
        chunk->size = size;
        kp_edi_chunk_list_push_back(&device->send_list, chunk);
    }

    return status;
}

static edi_status send_message(KPEDIState* device, void* message, size_t size)
{
    if(kp_edi_chunk_list_empty(&device->send_list))
    {
        const int status = nn_send(device->socket, &message, NN_MSG, NN_DONTWAIT);
        if(
            status == -1 &&
            (nn_errno() != EAGAIN || !enquene_outgoing_message(device, message, size))
            )
        {
            drop_outgoing_message(device, message);
            return edi_status_write_error;
        }
    }
    else if(!enquene_outgoing_message(device, message, size))
    {
        drop_outgoing_message(device, message);
        return edi_status_write_error;
    }

    return edi_status_success;
}


static edi_status disconnect(KPEDIState* device)
{
    if(device->connection_state == edi_connection_state_disconnected)
    {
        return edi_status_success;
    }

    int result;
    do
    {
        result = nn_close(device->socket);
        if(result == -1)
        {
            if(nn_errno() != EINTR)
            {
                trace_kp_edi_error_reason(device->name, "Unable to shutdown", nn_strerror(nn_errno()));
                return edi_status_unable_to_setup_connection;
            }

            trace_kp_edi_error(device->name, "Unable to shutdown, retrying");
        }
    }
    while(result == -1);

    device->socket = -1;
    device->connection_state = edi_connection_state_disconnected;
    const struct edi_communication_pair* mode = find_communication_mode(device->communication_mode);
    if(mode->read_unregister != NULL)
    {
        mode->read_unregister(device);
    }

    if(mode->write_unregister != NULL)
    {
        mode->write_unregister(device);
    }

    trace_kp_edi_disconnected(device->name);
    return edi_status_success;
}

static edi_status establish_connection_internal(
    KPEDIState* device,
    void* buffer,
    size_t size,
    int (*operation)(int socket, const char* address),
    const char* operation_name,
    const char* operation_completed_name
    )
{
    size_t string_size = strnlen(buffer, size);
    if(string_size == size)
    {
        trace_kp_edi_error(device->name, "Unable to connect. Address is not null terminated within specified length");
        return edi_status_invalid_block_size;
    }

    const struct edi_communication_pair* mode = find_communication_mode(device->communication_mode);
    int socket = nn_socket(AF_SP, mode->nanomsg_type);
    if(socket < 0)
    {
        trace_kp_edi_error_reason(device->name, "Unable to create socket", nn_strerror(nn_errno()));
        return edi_status_unable_to_setup_connection;
    }

    edi_status status;
    do
    {
        const int endpoint_id = operation(socket, buffer);
        if(endpoint_id < 0)
        {
            trace_kp_edi_error_operation(device->name, operation_name, (char*)buffer, nn_strerror(nn_errno()));

            status =  edi_status_unable_to_setup_connection;
            break;
        }

        fd_handle_type read_handle = -1;
        fd_handle_type write_handle = -1;

        if(
            mode->read_setup != NULL &&
            !mode->read_setup(device, socket, &read_handle)
            )
        {
            status = edi_status_unable_to_setup_connection;
            break;
        }

        if(
            mode->write_setup != NULL &&
            !mode->write_setup(device, socket, &write_handle)
            )
        {
            status = edi_status_unable_to_setup_connection;
            break;
        }

        if(device->connection_state == edi_connection_state_connected)
        {
            status = disconnect(device);
            if(status != edi_status_success)
            {
                break;
            }
        }

        device->connection_state = edi_connection_state_connected;
        device->socket_read_handle = read_handle;
        device->socket_write_handle = write_handle;
        device->socket = socket;
        socket = -1;
        if(mode->read_register != NULL)
        {
            mode->read_register(device);
        }

        trace_kp_edi_operation_completed(device->name, operation_completed_name, (const char*)buffer);

        status = edi_status_success;
    }
    while(0);

    if(socket != -1)
    {
        nn_close(socket);
    }

    return status;
}

static edi_status edi_connect(KPEDIState* device, void* buffer, size_t size)
{
    return establish_connection_internal(device, buffer, size, nn_connect, "connect", "connected");
}

static edi_status edi_bind(KPEDIState* device, void* buffer, size_t size)
{
    return establish_connection_internal(device, buffer, size, nn_bind, "bind", "bound");
}

static edi_status write_request(KPEDIState* device, void* buffer, size_t size)
{
    if(device->connection_state != edi_connection_state_connected)
    {
        trace_kp_edi_error(device->name, "Unable to write message. Device is disconnected");
        return edi_status_disconnected;
    }

    void* message_buffer = nn_allocmsg(size, 0);
    if(message_buffer == NULL)
    {
        trace_kp_edi_error(device->name, "Unable to send message due to allocation failure");
        return edi_status_write_error;
    }

    memcpy(message_buffer, buffer, size);
    return send_message(device, message_buffer, size);
}

static uint32_t edi_calculate_gather_size(const edi_message_part* list, uint32_t size)
{
    uint32_t total_size = 0;
    const edi_message_part* end = list + size;
    while(list != end)
    {
        total_size += list->size;
        ++list;
    }

    return total_size;
}

static bool gather_message(KPEDIState* device, const edi_message_part* list, uint32_t size, uint8_t* buffer)
{
    uint32_t offset = 0;
    for(int counter = 0; counter < size; ++counter)
    {
        const edi_message_part* entry = list + counter;
        hwaddr requested_size = entry->size;
        if(requested_size == 0)
        {
            continue;
        }

        void* mapped_buffer = cpu_physical_memory_map((hwaddr)entry->buffer, &requested_size, false);
        if(mapped_buffer == NULL)
        {
            trace_kp_edi_error_map_message_part(device->name, counter);
            return false;
        }

        memcpy(buffer + offset, mapped_buffer, requested_size);
        offset += requested_size;
        cpu_physical_memory_unmap(mapped_buffer, (hwaddr)entry->buffer, false, requested_size);
    }

    return true;
}

static edi_status handle_gather_write(KPEDIState* device, void* buffer, size_t size)
{
    if(device->connection_state != edi_connection_state_connected)
    {
        return edi_status_disconnected;
    }

    const edi_message_part* part_list = (const edi_message_part*) buffer;
    const uint32_t part_list_length = size / sizeof(edi_message_part);
    const uint32_t buffer_size = edi_calculate_gather_size(part_list, part_list_length);
    void* message_buffer = nn_allocmsg(buffer_size, 0);
    if(message_buffer == NULL)
    {
        trace_kp_edi_error(device->name, "Unable to send message due to allocation failure");
        return edi_status_write_error;
    }

    if(!gather_message(device, part_list, part_list_length, message_buffer))
    {
        nn_freemsg(message_buffer);
        trace_kp_edi_error(device->name, "Unable to gather all message parts");
        return edi_status_invalid_address;
    }

    return send_message(device, message_buffer, buffer_size);
}

static edi_status read_request(KPEDIState* device, void* buffer, size_t size)
{
    kp_edi_message_chunk* chunk = kp_edi_chunk_list_front(&device->receive_list);
    if(chunk == NULL)
    {
        device->registers.size = 0;
        return edi_status_success;
    }

    void* message_data = chunk->message;
    size_t message_size = min_size_t(chunk->size, size);
    memcpy(buffer, message_data, message_size);
    device->registers.size = message_size;
    return edi_status_success;
}

static edi_status handle_scatter_read(KPEDIState* device, void* buffer, size_t size)
{
    kp_edi_message_chunk* chunk = kp_edi_chunk_list_front(&device->receive_list);
    if(chunk == NULL)
    {
        device->registers.size = 0;
        return edi_status_success;
    }

    const uint8_t* messagebuffer = (const uint8_t*)chunk->message;
    edi_message_part* part_list = (edi_message_part*) buffer;
    const uint32_t part_list_length = size / sizeof(edi_message_part);

    uint32_t offset = 0;
    uint32_t counter = 0;
    edi_status result = edi_status_success;
    for(; counter < part_list_length; ++counter)
    {
        edi_message_part* entry = part_list + counter;
        hwaddr requested_size = entry->size;
        if(requested_size == 0)
        {
            continue;
        }

        void* mapped_buffer = cpu_physical_memory_map(entry->buffer, &requested_size, true);
        if(mapped_buffer == NULL)
        {
            trace_kp_edi_error_map_message_part(device->name, counter);
            result = edi_status_invalid_address;
            break;
        }

        size_t part_size = min_size_t(chunk->size - offset, requested_size);
        memcpy(mapped_buffer, messagebuffer + offset, part_size);
        entry->size = part_size;
        offset += requested_size;
        cpu_physical_memory_unmap(mapped_buffer, entry->buffer, true, requested_size);
        if(part_size != requested_size)
        {
            ++counter;
            break;
        }
    }

    device->registers.size = counter * sizeof(edi_message_part);
    return result;
}

static edi_status handle_remove(KPEDIState* device)
{
    kp_edi_message_chunk* chunk = kp_edi_chunk_list_pop_front(&device->receive_list);
    if(chunk == NULL)
    {
        trace_kp_edi_message_remove_nothing(device->name);
    }
    else
    {
        kp_edi_destroy_message_chunk(chunk);
        trace_kp_edi_message_removed(device->name, device->receive_list.size);
    }

    return edi_status_success;
}

static edi_status handle_connection_type(KPEDIState* device, void* buffer, size_t size)
{
    if(size != sizeof(int))
    {
        trace_kp_edi_error_invalid_block_size(device->name, sizeof(int), size);
        return edi_status_invalid_block_size;
    }

    const edi_communication_mode input_mode = (edi_communication_mode)*(int*)buffer;
    const struct edi_communication_pair* mode = find_communication_mode(input_mode);
    if(mode->nanomsg_type == -1)
    {
        trace_kp_edi_error_invalid_connection_mode(device->name, (int)input_mode);
        return edi_status_invalid_communication_mode;
    }

    device->communication_mode = input_mode;
    trace_kp_edi_connection_type_set(device->name, mode->name);

    return edi_status_success;
}

static edi_status handle_query_count(KPEDIState* device)
{
    device->registers.size = device->receive_list.size;
    return edi_status_success;
}

static edi_status handle_query_size(KPEDIState* device)
{
    size_t size = 0;
    kp_edi_message_chunk* chunk = kp_edi_chunk_list_front(&device->receive_list);
    if(chunk != NULL)
    {
        size = chunk->size;
    }

    device->registers.size = size;
    return edi_status_success;
}

static const char* command_names[] =
{
    "none",
    "connect",
    "bind",
    "disconnect",
    "write",
    "write gather",
    "read",
    "read scatter",
    "query message count",
    "query message size",
    "remove message ",
    "set connection type"
};

#define COMMAND_MAP_SIZE (sizeof(command_names) / sizeof(*command_names))

_Static_assert((int)edi_command_none == 0, "fix command_names map");
_Static_assert((int)edi_command_connect == 1, "fix command_names map");
_Static_assert((int)edi_command_bind == 2, "fix command_names map");
_Static_assert((int)edi_command_disconnect == 3, "fix command_names map");
_Static_assert((int)edi_command_write == 4, "fix command_names map");
_Static_assert((int)edi_command_write_gather == 5, "fix command_names map");
_Static_assert((int)edi_command_read == 6, "fix command_names map");
_Static_assert((int)edi_command_read_scatter == 7, "fix command_names map");
_Static_assert((int)edi_command_query_message_count == 8, "fix command_names map");
_Static_assert((int)edi_command_query_message_size == 9, "fix command_names map");
_Static_assert((int)edi_command_remove_message == 10, "fix command_names map");
_Static_assert((int)edi_command_set_connection_type == 11, "fix command_names map");

static const char* map_command_code_to_string(edi_command command)
{
    if((int)command < COMMAND_MAP_SIZE)
    {
        return command_names[(int)command];
    }
    else
    {
        return "invalid";
    }
}

static edi_status handle_request_with_data(
    KPEDIState* device,
    bool is_writable,
    edi_status (*handler)(KPEDIState* state, void* buffer, size_t size)
    )
{
    hwaddr address = device->registers.pointer;
    hwaddr size = device->registers.size;
    hwaddr requested_size = size;
    void* mapped_buffer = NULL;
    if(requested_size > 0)
    {
        mapped_buffer = cpu_physical_memory_map(address, &requested_size, is_writable);
        if(mapped_buffer == NULL)
        {
            trace_kp_edi_error_unable_to_map_address(device->name, address);
            return edi_status_invalid_address;
        }
    }

    edi_status result = edi_status_invalid_block_size;
    if(requested_size == size)
    {
        result = handler(device, mapped_buffer, size);
    }

    if(requested_size > 0)
    {
        cpu_physical_memory_unmap(mapped_buffer, address, is_writable, requested_size);
    }

    return result;
}

edi_status kp_edi_handle_command(KPEDIState* state, edi_command command)
{
    trace_kp_edi_command(state->name, map_command_code_to_string(command));

    switch((edi_command)command)
    {
    case edi_command_none:
        return edi_status_success;
    case edi_command_connect:
        return handle_request_with_data(state, false, edi_connect);
    case edi_command_bind:
        return handle_request_with_data(state, false, edi_bind);
    case edi_command_disconnect:
        return disconnect(state);
    case edi_command_write:
        return handle_request_with_data(state, false, write_request);
    case edi_command_write_gather:
        return handle_request_with_data(state, false, handle_gather_write);
    case edi_command_read:
        return handle_request_with_data(state, true, read_request);
    case edi_command_read_scatter:
        return handle_request_with_data(state, true, handle_scatter_read);
    case edi_command_query_message_count:
        return handle_query_count(state);
    case edi_command_query_message_size:
        return handle_query_size(state);
    case edi_command_remove_message:
        return handle_remove(state);
    case edi_command_set_connection_type:
        return handle_request_with_data(state, false, handle_connection_type);
    default:
        trace_kp_edi_error_invalid_command(state->name, command);
        return edi_status_invalid_command;
    }
}