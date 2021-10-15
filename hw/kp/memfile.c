#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/core/cpu.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qapi/error.h"
#ifdef CONFIG_WIN32
#include <windows.h>
#endif

typedef struct KPMemfileState {
    DeviceState parent_obj;

    uint64_t addr;
    uint64_t region_length;
    char* file;
    bool read_only;

#ifdef CONFIG_WIN32
    HANDLE fileHandle;
    void* ptr;
#endif
} KPMemfileState;

#define TYPE_KP_MEMFILE "kp-memfile"
#define KP_MEMFILE(obj) OBJECT_CHECK(KPMemfileState, (obj), \
                                         TYPE_KP_MEMFILE)

static void kp_memfile_realize(DeviceState *dev, Error **errp)
{
    KPMemfileState *s = KP_MEMFILE(dev);

    MemoryRegion *system_memory = get_system_memory();

    char* regionName = object_get_canonical_path(OBJECT(dev));

#ifdef CONFIG_POSIX
    MemoryRegion *region = g_new(MemoryRegion, 1);
    memory_region_init_ram_from_file(region, NULL, regionName, s->region_length,
        0,
        RAM_SHARED,
        s->file,
        s->read_only,
        errp
    );
#elif CONFIG_WIN32
    s->fileHandle = CreateFile(
        s->file,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if(s->fileHandle == INVALID_HANDLE_VALUE) {
        error_setg_win32(errp, GetLastError(), "failed to open file '%s'",
                         s->file);
        return;
    }

    DWORD sizeHigh = (DWORD)((s->region_length >> 32) & 0xFFFFFFFFL);
    DWORD sizeLow = (DWORD)(s->region_length & 0xFFFFFFFFL);

    HANDLE fileMapping = CreateFileMapping(
        s->fileHandle,
        NULL,
        PAGE_READWRITE,
        sizeHigh,
        sizeLow,
        NULL
    );

    if(fileMapping == INVALID_HANDLE_VALUE) {
        error_setg_win32(errp, GetLastError(), "failed to create file mapping");
        CloseHandle(s->fileHandle);
        s->fileHandle = INVALID_HANDLE_VALUE;
        return;
    }

    s->ptr = MapViewOfFile(fileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, s->region_length);
    CloseHandle(fileMapping);

    if(s->ptr == NULL) {
        error_setg_win32(errp, GetLastError(), "failed to map file view");
        CloseHandle(s->fileHandle);
        s->fileHandle = INVALID_HANDLE_VALUE;
        return;
    }
    
    MemoryRegion *region = g_new(MemoryRegion, 1);
    memory_region_init_ram_ptr(region, NULL, regionName, s->region_length, s->ptr);
    memory_region_set_readonly(region, s->read_only);
#else
#error "Don't know how to build kp-memfile"
#endif

    memory_region_add_subregion(system_memory, s->addr, region);
}

static void kp_memfile_unrealize(DeviceState *dev)
{
#ifdef CONFIG_WIN32
    KPMemfileState *s = KP_MEMFILE(dev);

    FlushViewOfFile(s->ptr, 0);
    UnmapViewOfFile(s->ptr);
    CloseHandle(s->fileHandle);
#endif
}

static Property kp_memfile_props[] = {
    DEFINE_PROP_UINT64("addr", KPMemfileState, addr, 0x60000000),
    DEFINE_PROP_UINT64("len", KPMemfileState, region_length, 0),
    DEFINE_PROP_STRING("file", KPMemfileState, file),
    DEFINE_PROP_BOOL("read-only", KPMemfileState, read_only, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void kp_memfile_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, kp_memfile_props);

    dc->realize = kp_memfile_realize;
    dc->unrealize = kp_memfile_unrealize;

    dc->desc = "KP Labs - File-backed memory region";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_memfile_info = {
    .name = TYPE_KP_MEMFILE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPMemfileState),
    .class_init = kp_memfile_class_init,
};

static void kp_memfile_register_type(void)
{
    type_register_static(&kp_memfile_info);
}

type_init(kp_memfile_register_type)
