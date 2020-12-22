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
#include "hw/qdev-clock.h"
#include "migration/vmstate.h"
#include "hw/misc/unimp.h"
#include "qapi/visitor.h"
#include "cpu.h"

typedef struct  {
    MachineState parent_obj;

    Clock *cpuclk;

    int64_t flash_size_kb;
    int64_t freq_mhz;
    int64_t num_irq;
    bool writable_code_memory;
} VirtCortexMMachineState;

#define TYPE_VIRT_CORTEX_M_MACHINE MACHINE_TYPE_NAME("virt_cortex_m")

#define VIRT_CORTEX_M_MACHINE(obj) \
    OBJECT_CHECK(VirtCortexMMachineState, obj, TYPE_VIRT_CORTEX_M_MACHINE)

static void virt_cortex_m_init(MachineState *ms)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(ms);

    DeviceState *nvic;
    int sram_size;
    int flash_size;

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    flash_size = m->flash_size_kb * 1024;
    sram_size = m->parent_obj.ram_size;

    if(m->writable_code_memory)
    {
        memory_region_init_ram(flash, NULL, "virt_cortex_m.flash", flash_size, &error_fatal);
    }
    else
    {
        memory_region_init_rom(flash, NULL, "virt_cortex_m.flash", flash_size, &error_fatal);
    }

    memory_region_add_subregion(system_memory, 0, flash);

    memory_region_init_ram(sram, NULL, "virt_cortex_m.sram", sram_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x20000000, sram);

    m->cpuclk = clock_new(OBJECT(m), "cpuclk");
    clock_set_hz(m->cpuclk, m->freq_mhz * 1000 * 1000);

    nvic = qdev_new(TYPE_ARMV7M);

    qdev_prop_set_uint32(nvic, "num-irq", m->num_irq);
    qdev_prop_set_string(nvic, "cpu-type", ms->cpu_type);
    qdev_prop_set_bit(nvic, "enable-bitband", true);
    object_property_set_link(OBJECT(nvic), "memory", OBJECT(get_system_memory()),
                                     &error_abort);
    qdev_connect_clock_in(nvic, "cpuclk", m->cpuclk);
    qdev_connect_clock_in(nvic, "refclk", m->cpuclk);

    if (!sysbus_realize(SYS_BUS_DEVICE(nvic), &error_fatal)) {
        return;
    }

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

static void writable_code_memory_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_bool(v, name, &m->writable_code_memory, errp);
}

static void writable_code_memory_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    visit_type_bool(v, name, &m->writable_code_memory, errp);
}

static void virt_cortex_m_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Virtual ARM Cortex-M";
    mc->init = virt_cortex_m_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m3");

    object_class_property_add(
        oc, "flash-kb", "int",
        &flash_size_get, &flash_size_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "flash-kb",
        "Flash size in KB"
    );

    object_class_property_add(
        oc, "freq-mhz", "int",
        &freq_mhz_get, &freq_mhz_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "freq-mhz",
        "MCU frequency in MHz"
    );

    object_class_property_add(
        oc, "num-irq", "int",
        &num_irq_get, &num_irq_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "num-irq",
        "Number of IRQs"
    );

    object_class_property_add(
        oc, "writable-code-memory", "bool",
        &writable_code_memory_get, &writable_code_memory_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "writable-code-memory",
        "Flag indicating whether code memory is freely writable"
    );
}

static void virt_cortex_m_instance_init(Object* obj)
{
    VirtCortexMMachineState* m = VIRT_CORTEX_M_MACHINE(obj);
    m->flash_size_kb = 1 * 1024;
    m->freq_mhz = 50;
    m->num_irq = 64;
    m->writable_code_memory = false;
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