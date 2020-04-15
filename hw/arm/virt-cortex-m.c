#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "hw/i2c/i2c.h"
#include "net/net.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/char/pl011.h"
#include "hw/input/gamepad.h"
#include "hw/irq.h"
#include "hw/watchdog/cmsdk-apb-watchdog.h"
#include "migration/vmstate.h"
#include "hw/misc/unimp.h"
#include "qapi/visitor.h"
#include "cpu.h"

typedef struct  {
    MachineState parent_obj;

    int64_t flash_size_kb;
    int64_t freq_mhz;
    int64_t num_irq;
} VirtCortexMMachineState;

#define TYPE_VIRT_CORTEX_M_MACHINE MACHINE_TYPE_NAME("virt_cortex_m")

#define VIRT_CORTEX_M_MACHINE(obj) \
    OBJECT_CHECK(VirtCortexMMachineState, obj, TYPE_VIRT_CORTEX_M_MACHINE)

// static
// void do_sys_reset(void *opaque, int n, int level)
// {
//     if (level) {
//         qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
//     }
// }

static void virt_cortex_m_init(MachineState *ms)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(ms);

    DeviceState *nvic;
    int sram_size;
    int flash_size;
    // DeviceState *dev;

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    qemu_log("RAM size: %ld\n", m->parent_obj.ram_size);
    qemu_log("Flash size: %ld KB\n", m->flash_size_kb);
    qemu_log("Freq %ldMHz\n", m->freq_mhz);
    qemu_log("Num irq %ld\n", m->num_irq);

    flash_size = m->flash_size_kb * 1024;
    sram_size = m->parent_obj.ram_size;

    memory_region_init_rom(flash, NULL, "virt_cortex_m.flash", flash_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0, flash);

    memory_region_init_ram(sram, NULL, "virt_cortex_m.sram", sram_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x20000000, sram);

    nvic = qdev_new(TYPE_ARMV7M);
    qemu_log("NVIC (board_init): %p\n", nvic);
    qdev_prop_set_uint32(nvic, "num-irq", m->num_irq);
    qdev_prop_set_string(nvic, "cpu-type", ms->cpu_type);
    qdev_prop_set_bit(nvic, "enable-bitband", true);
    object_property_set_link(OBJECT(nvic), "memory", OBJECT(get_system_memory()),
                                     &error_abort);
    /* This will exit with an error if the user passed us a bad cpu_type */
    // qdev_init_nofail(nvic);

    if (!sysbus_realize(SYS_BUS_DEVICE(nvic), &error_fatal)) {
        return;
    }

    // qdev_connect_gpio_out_named(nvic, "SYSRESETREQ", 0,
    //                             qemu_allocate_irq(&do_sys_reset, NULL, 0));

    system_clock_scale = NANOSECONDS_PER_SECOND / (m->freq_mhz * 1000 * 1000);
    armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, flash_size);
}

static void flash_size_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_int64(v, name, &m->flash_size_kb, errp);
}

static void flash_size_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_int64(v, name, &m->flash_size_kb, errp);
}

static void freq_mhz_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_int64(v, name, &m->freq_mhz, errp);
}

static void freq_mhz_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_int64(v, name, &m->freq_mhz, errp);
}

static void num_irq_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_int64(v, name, &m->num_irq, errp);
}

static void num_irq_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_int64(v, name, &m->num_irq, errp);
}

static void virt_cortex_m_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Virtual ARM Cortex-M";
    mc->init = virt_cortex_m_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m3");

    object_class_property_add(
        oc, "flash_kb", "int",
        &flash_size_get, &flash_size_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "flash_kb",
        "Flash size in KB"
    );

    object_class_property_add(
        oc, "freq_mhz", "int",
        &freq_mhz_get, &freq_mhz_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "freq_mhz",
        "MCU frequency in MHz"
    );

    object_class_property_add(
        oc, "num_irq", "int",
        &num_irq_get, &num_irq_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "num_irq",
        "Number of IRQs"
    );
}

static void virt_cortex_m_instance_init(Object* obj)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    m->flash_size_kb = 1 * 1024;
    m->freq_mhz = 50;
    m->num_irq = 64;
}

static const TypeInfo virt_cortex_m_type = {
    .name = TYPE_VIRT_CORTEX_M_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(VirtCortexMMachineState),
    .class_init = virt_cortex_m_class_init,
    .instance_init = virt_cortex_m_instance_init,
};


static void virt_cortex_m_machine_init(void)
{
    type_register_static(&virt_cortex_m_type);
}

type_init(virt_cortex_m_machine_init)