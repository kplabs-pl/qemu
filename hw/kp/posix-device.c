#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/boards.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "trace/trace-hw_kp.h"
#include "chardev/char-fe.h"

static int OpenModeMapping[12] = {
    O_RDONLY,
    O_RDONLY | O_BINARY,
    O_RDWR,
    O_RDWR | O_BINARY,
    O_WRONLY | O_CREAT | O_TRUNC,
    O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
    O_RDWR | O_CREAT | O_TRUNC,
    O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
    O_WRONLY | O_CREAT | O_APPEND,
    O_WRONLY | O_CREAT | O_APPEND | O_BINARY,
    O_RDWR | O_CREAT | O_APPEND,
    O_RDWR | O_CREAT | O_APPEND | O_BINARY
};

typedef enum FileType {
    FILE_TYPE_UNUSED = 0,
    FILE_TYPE_NORMAL = 1,
    FILE_TYPE_CONSOLE = 2,
} FileType;

typedef struct GuestFile {
    FileType type;
    int hostFD;
} GuestFile;

#define MAX_FD_COUNT 256

typedef struct KPPosixState {
    DeviceState parent_obj;

    uint64_t addr;
    CharBackend debugChannel;

    GuestFile fileDescriptors[MAX_FD_COUNT];
} KPPosixState;

#define TYPE_KP_POSIX "kp-posix"
#define KP_POSIX(obj) OBJECT_CHECK(KPPosixState, (obj), \
                                         TYPE_KP_POSIX)


typedef struct KPPosixCommandBlock {
    uint32_t commandCode;
    uint32_t registers[7];
} KPPosixCommandBlock;

typedef void(*KPPosixCommandHandler)(KPPosixState* state, KPPosixCommandBlock* block);

static int allocate_fd(KPPosixState* state, FileType type, int hostFD)
{
    for(int i = 0; i < MAX_FD_COUNT; i++) {
        if(state->fileDescriptors[i].type == FILE_TYPE_UNUSED) {
            state->fileDescriptors[i].type = type;
            state->fileDescriptors[i].hostFD = hostFD;
            return i + 1;
        }
    }

    return -1;
}

static void deallocate_fd(KPPosixState* state, int guestFD)
{
    if(guestFD > MAX_FD_COUNT)
    {
        return;
    }

    if(guestFD < 0)
    {
        return;
    }

    GuestFile* f = &state->fileDescriptors[guestFD - 1];
    f->hostFD = 0;
    f->type = FILE_TYPE_UNUSED;
}

static GuestFile* lookup_fd(KPPosixState* state, int guestFD)
{
    if(guestFD > MAX_FD_COUNT)
    {
        return NULL;
    }

    if(guestFD < 0)
    {
        return NULL;
    }

    GuestFile* f = &state->fileDescriptors[guestFD - 1];
    if(f->type == FILE_TYPE_UNUSED) 
    {
        return NULL;
    }

    return f;
}

static char* read_guest_string(hwaddr address, hwaddr stringLength)
{
    hwaddr actualStringLength = stringLength;
    void* buffer = cpu_physical_memory_map(address, &actualStringLength, false);

    if(buffer == NULL)
    {
        return NULL;
    }

    if(actualStringLength != stringLength)
    {
        cpu_physical_memory_unmap(buffer, address, false, actualStringLength);
        return NULL;
    }

    char* stringPtr = g_strndup(buffer, actualStringLength);

    cpu_physical_memory_unmap(buffer, address, false, actualStringLength);

    return stringPtr;
}

static void kp_posix_exit(KPPosixState* state, KPPosixCommandBlock* command)
{
    exit(command->registers[0]);
}

static void kp_posix_debug_out(KPPosixState* state, KPPosixCommandBlock* command)
{
    hwaddr textAddress = command->registers[0];
    hwaddr textLength = command->registers[1];

    void* buffer = cpu_physical_memory_map(textAddress, &textLength, false);

    if(qemu_chr_fe_backend_connected(&state->debugChannel))
    {
        qemu_chr_fe_write_all(&state->debugChannel, buffer, textLength);
    }
    else
    {
        const char* mappedText = buffer;
        fwrite(mappedText, textLength, 1, stderr);
        fflush(stderr);
    }

    cpu_physical_memory_unmap(buffer, textAddress, false, textLength);
}

static void kp_posix_open_file(KPPosixState* state, KPPosixCommandBlock* command)
{
    hwaddr fileNameAddress = command->registers[0];
    hwaddr fileNameLength = command->registers[1];
    int openMode = command->registers[2];

    char* fileName = read_guest_string(fileNameAddress, fileNameLength);

    int fd = -1;
    FileType type = FILE_TYPE_UNUSED;

    if(strcmp(fileName, ":tt") == 0)
    {
        if(4 <= openMode && openMode <= 7) 
        {
            fd = STDOUT_FILENO;
            type = FILE_TYPE_CONSOLE;
        }
        else if(8 <= openMode && openMode <= 11) 
        {
            fd = STDERR_FILENO;
            type = FILE_TYPE_CONSOLE;
        }
        else 
        {
            fd = -1;
            errno = EBADF;
        }
    }
    else 
    {
        fd = open(fileName, OpenModeMapping[openMode], 0644);
        type = FILE_TYPE_NORMAL;
    }

    if(fd >= 0)
    {
        int guestFD = allocate_fd(state, type, fd);
        
        if(guestFD < 0)
        {
            command->registers[0] = ENOMEM;
            command->registers[1] = 0;
        }
        else
        {
            command->registers[0] = 0;
            command->registers[1] = guestFD;
        }
    }
    else
    {
        command->registers[0] = errno;
        command->registers[1] = 0;
    }

    g_free(fileName);
}

static void kp_posix_write_file(KPPosixState* state, KPPosixCommandBlock* command)
{
    int guestFD = command->registers[0];
    hwaddr guestBufferAddress = command->registers[1];
    hwaddr guestBufferLength = command->registers[2];

    GuestFile* hostFile = lookup_fd(state, guestFD);
    if(hostFile == NULL) 
    {
        command->registers[0] = EBADF;
        command->registers[1] = 0;
        return;
    }

    void* buffer = cpu_physical_memory_map(guestBufferAddress, &guestBufferLength, false);

    ssize_t n = write(hostFile->hostFD, buffer, guestBufferLength);

    cpu_physical_memory_unmap(buffer, guestBufferAddress, false, guestBufferLength);

    if(n < 0)
    {
        command->registers[0] = -errno;
        command->registers[1] = 0;
    }
    else
    {
        command->registers[0] = 0;
        command->registers[1] = n;
    }
}

static void kp_posix_read_file(KPPosixState* state, KPPosixCommandBlock* command)
{
    int guestFD = command->registers[0];
    hwaddr guestBufferAddress = command->registers[1];
    hwaddr guestBufferLength = command->registers[2];

    GuestFile* hostFile = lookup_fd(state, guestFD);
    if(hostFile == NULL) 
    {
        command->registers[0] = EBADF;
        command->registers[1] = 0;
        return;
    }

    void* buffer = cpu_physical_memory_map(guestBufferAddress, &guestBufferLength, true);

    ssize_t n = read(hostFile->hostFD, buffer, guestBufferLength);

    cpu_physical_memory_unmap(buffer, guestBufferAddress, true, guestBufferLength);

    if(n < 0)
    {
        command->registers[0] = -errno;
        command->registers[1] = 0;
    }
    else
    {
        command->registers[0] = 0;
        command->registers[1] = n;
    }
}

static void kp_posix_seek_file(KPPosixState* state, KPPosixCommandBlock* command)
{
    int guestFD = command->registers[0];
    int offset = command->registers[1];

    GuestFile* hostFile = lookup_fd(state, guestFD);
    if(hostFile == NULL) 
    {
        command->registers[0] = EBADF;
        command->registers[1] = 0;
        return;
    }

    int r = lseek(hostFile->hostFD, offset, SEEK_SET);

    if(r < 0)
    {
        command->registers[0] = errno;
    }
    else 
    {
        command->registers[0] = 0;
    }
}

static void kp_posix_close_file(KPPosixState* state, KPPosixCommandBlock* command)
{
    int guestFD = command->registers[0];
    GuestFile* hostFile = lookup_fd(state, guestFD);

    int err = 0;
    if(hostFile != NULL)
    {
        if(hostFile->type == FILE_TYPE_NORMAL) 
        {
            err = close(hostFile->hostFD);
        }
        else
        {
            // ignore closing of console file
            err = 0;
        }
    }
    else
    {
        err = EBADF;
    }

    if(err != 0) 
    {
        command->registers[0] = errno;
    } 
    else 
    {
        deallocate_fd(state, guestFD);
        command->registers[0] = 0;
    }
}

static void kp_posix_remove_file(KPPosixState* state, KPPosixCommandBlock* command)
{
    hwaddr fileNameAddress = command->registers[0];
    hwaddr fileNameLength = command->registers[1];

    char* fileName = read_guest_string(fileNameAddress, fileNameLength);

    int n = unlink(fileName);

    if(n != 0)
    {
        command->registers[0] = errno;
    }
    else
    {
        command->registers[0] = 0;
    }

    g_free(fileName);
}

static void kp_posix_get_cmdline(KPPosixState* state, KPPosixCommandBlock* command)
{
    hwaddr guestBufferAddress = command->registers[0];
    hwaddr guestBufferLength = command->registers[1];
    size_t offset = command->registers[2];

    gchar* cmdline;
    if(strlen(current_machine->kernel_cmdline) > 0)
    {
        char* args[] = {current_machine->kernel_filename, current_machine->kernel_cmdline, NULL};
        cmdline = g_strjoinv(" ", args);
    }
    else
    {
        cmdline = g_strdup(current_machine->kernel_filename);;
    }
    
    if (cmdline == NULL)
    {
        command->registers[0] = 0;
        g_free(cmdline);
        return;
    }

    size_t cmdlineLength = strlen(cmdline);
    if (cmdlineLength == 0 || guestBufferLength == 0 || offset >= cmdlineLength)
    {
        command->registers[0] = 0;
        g_free(cmdline);
        return;
    }

    char* buffer = cpu_physical_memory_map(guestBufferAddress, &guestBufferLength, true);

    size_t bytesCount = MIN(cmdlineLength - offset + 1, guestBufferLength);
    memcpy(buffer, cmdline + offset, bytesCount);

    cpu_physical_memory_unmap(buffer, guestBufferAddress, true, guestBufferLength);
    command->registers[0] = bytesCount;

    g_free(cmdline);
}

#define COMMAND_HANDLER_COUNT 32
static KPPosixCommandHandler CommandHandlers[COMMAND_HANDLER_COUNT] = {
    [1] = &kp_posix_exit,
    [2] = &kp_posix_debug_out,
    [3] = &kp_posix_open_file,
    [4] = &kp_posix_write_file,
    [5] = &kp_posix_read_file,
    [6] = &kp_posix_seek_file,
    [7] = &kp_posix_close_file,
    [8] = &kp_posix_remove_file,
    [9] = &kp_posix_get_cmdline,
};

static void kp_posix_handle_command(KPPosixState* state, hwaddr commandBlockAddress)
{
    hwaddr commandBlockLength = 8 * 4;
    uint32_t* buffer = cpu_physical_memory_map(commandBlockAddress, &commandBlockLength, true);

    if(buffer == NULL || commandBlockLength != 8 * 4) 
    {
        qemu_log_mask(LOG_GUEST_ERROR, "kp-posix: Failed to map command block at address %zu\n", commandBlockAddress);
        cpu_physical_memory_unmap(buffer, commandBlockAddress, true, commandBlockLength);
        return;
    }

    KPPosixCommandBlock commandBlock;
    commandBlock.commandCode = ldl_p(buffer + 0);
    commandBlock.registers[0] = ldl_p(buffer + 1);
    commandBlock.registers[1] = ldl_p(buffer + 2);
    commandBlock.registers[2] = ldl_p(buffer + 3);
    commandBlock.registers[3] = ldl_p(buffer + 4);
    commandBlock.registers[4] = ldl_p(buffer + 5);
    commandBlock.registers[5] = ldl_p(buffer + 6);
    commandBlock.registers[6] = ldl_p(buffer + 7);
    
    if(commandBlock.commandCode >= COMMAND_HANDLER_COUNT)
    {
        qemu_log_mask(LOG_GUEST_ERROR, "kp-posix: Attempted to execute unrecognized command %d\n", commandBlock.commandCode);
        goto end;
    }

    KPPosixCommandHandler handler = CommandHandlers[commandBlock.commandCode];

    if(handler == NULL)
    {
        qemu_log_mask(LOG_GUEST_ERROR, "kp-posix: Attempted to execute unrecognized command %d\n", commandBlock.commandCode);
        goto end;
    }

    handler(state, &commandBlock);

    stl_p(buffer + 1, commandBlock.registers[0]);
    stl_p(buffer + 2, commandBlock.registers[1]);
    stl_p(buffer + 3, commandBlock.registers[2]);
    stl_p(buffer + 4, commandBlock.registers[3]);
    stl_p(buffer + 5, commandBlock.registers[4]);
    stl_p(buffer + 6, commandBlock.registers[5]);
    stl_p(buffer + 7, commandBlock.registers[6]);

end:
    cpu_physical_memory_unmap(buffer, commandBlockAddress, true, commandBlockLength);
}

static uint64_t kp_posix_register_read(void* opaque, hwaddr addr, unsigned size)
{
    switch(addr)
    {
        case 0:
            return 0x50534958; 
        default:
            return 0;
    }

    return 0;
}

static void kp_posix_register_write(void* opaque, hwaddr addr, uint64_t adata, unsigned size)
{
    KPPosixState* s = KP_POSIX(opaque);

    switch(addr)
    {
        case 0:
            // ignore write to ID register
            break;
        case 4:
            kp_posix_handle_command(s, adata);
            break;
        default: 
            break;
    }

}

static const MemoryRegionOps RegisterOps = {
    .read = kp_posix_register_read,
    .write = kp_posix_register_write,
};

static void kp_posix_realize(DeviceState* dev, Error** errp)
{
    KPPosixState* s = KP_POSIX(dev);

    memset(s->fileDescriptors, 0, sizeof(s->fileDescriptors));

    MemoryRegion* system_memory = get_system_memory();
    MemoryRegion* region = g_new(MemoryRegion, 1);

    memory_region_init_io(region, NULL, &RegisterOps, s, "kp-posix", 16 * 4);
    memory_region_add_subregion(system_memory, s->addr, region);
}

static Property kp_posix_props[] = {
    DEFINE_PROP_UINT64("addr", KPPosixState, addr, 0xF0040010),
    DEFINE_PROP_CHR("chardev", KPPosixState, debugChannel),

    DEFINE_PROP_END_OF_LIST(),
};

static void kp_posix_class_init(ObjectClass* klass, void* data)
{
    DeviceClass* dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, kp_posix_props);

    dc->realize = kp_posix_realize;
    dc->desc = "KP Labs - Posix Functions Device";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_posix_info = {
    .name = TYPE_KP_POSIX,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPPosixState),
    .class_init = kp_posix_class_init,
};

static void kp_posix_register_type(void)
{
    type_register_static(&kp_posix_info);
}

type_init(kp_posix_register_type)
