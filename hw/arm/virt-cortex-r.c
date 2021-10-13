#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/intc/arm_gic.h"
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

    struct arm_boot_info bootinfo;

    ARMCPU cpu;
    GICState gic;

    MemoryRegion* gic_region_dist;
    MemoryRegion* gic_region_cpu;

    int64_t freq_mhz;
    int64_t num_irq;
    ram_addr_t flash_size_kb;

    qemu_irq timer_irq;
} VirtCortexRMachineState;

#define TYPE_VIRT_CORTEX_R_MACHINE MACHINE_TYPE_NAME("virt_cortex_r")

#define VIRT_CORTEX_R_MACHINE(obj) \
    OBJECT_CHECK(VirtCortexRMachineState, obj, TYPE_VIRT_CORTEX_R_MACHINE)

#define GIC_BASE_ADDR       0xf9000000
#define GIC_DIST_ADDR       0xf9010000
#define GIC_CPU_ADDR        0xf9020000

static void virt_cortex_r_init(MachineState *ms)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(ms);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    m->gic_region_dist = g_new(MemoryRegion, 1);
    m->gic_region_cpu = g_new(MemoryRegion, 1);

    int flash_size = m->flash_size_kb * 1024;
    int sram_size = m->parent_obj.ram_size;

    memory_region_init_rom(flash, NULL, "virt_cortex_r.flash", flash_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x00000000, flash);

    memory_region_init_ram(sram, NULL, "virt_cortex_r.sram", sram_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x08000000, sram);

    object_initialize_child(OBJECT(m), "cpu",
                            &m->cpu,
                            m->parent_obj.cpu_type);

    object_property_set_bool(OBJECT(&m->cpu), "reset-hivecs", false, &error_abort);

    object_initialize_child(OBJECT(m), "gic", &m->gic, "arm_gic");
    qdev_prop_set_uint32(DEVICE(&m->gic), "num-irq", m->num_irq);

    if (!qdev_realize(DEVICE(&m->cpu), NULL, &error_abort)) {
        return;
    }

    if (!sysbus_realize(SYS_BUS_DEVICE(&m->gic), &error_abort)) {
        return;
    }

    {
        m->timer_irq = qdev_get_gpio_in(DEVICE(&m->gic), 0);

        SysBusDevice* timer = SYS_BUS_DEVICE(qdev_new("sp804"));

        object_property_set_uint(OBJECT(timer), "freq0", m->freq_mhz * 1000000, &error_abort);
        object_property_set_uint(OBJECT(timer), "freq1", m->freq_mhz * 1000000, &error_abort);

        sysbus_realize_and_unref(timer, &error_fatal);
        sysbus_mmio_map(timer, 0, 0xFC000000);
        sysbus_connect_irq(timer, 0, m->timer_irq);
    }

    {
        // connect GIC to memory (distributor)
        MemoryRegion* gic_region = sysbus_mmio_get_region(SYS_BUS_DEVICE(&m->gic), 0); // GIC_DIST_ADDR
        MemoryRegion* alias = m->gic_region_dist;

        memory_region_init_alias(alias, OBJECT(m), "gic-alias-dist", gic_region,
                                     0x0, 0x1000);

        memory_region_add_subregion(system_memory, GIC_DIST_ADDR, alias);
    }

    {
        // connect GIC to memory (CPU)
        MemoryRegion* gic_region = sysbus_mmio_get_region(SYS_BUS_DEVICE(&m->gic), 1); // GIC_CPU_ADDR
        MemoryRegion* alias = m->gic_region_cpu;

        memory_region_init_alias(alias, OBJECT(m), "gic-alias-cpu", gic_region,
                                     0, 0x1000);

        memory_region_add_subregion(system_memory, GIC_CPU_ADDR, alias);

    }

    {
        // connect GIC GPIO to CPU
        sysbus_connect_irq(SYS_BUS_DEVICE(&m->gic), 0,
                           qdev_get_gpio_in(DEVICE(&m->cpu),
                                            ARM_CPU_IRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&m->gic), 1,
                           qdev_get_gpio_in(DEVICE(&m->cpu),
                                            ARM_CPU_FIQ));
    }

    memset(&m->bootinfo, 0, sizeof(m->bootinfo));
    m->bootinfo.ram_size = sram_size;

    arm_load_kernel(&m->cpu, ms, &m->bootinfo);
}

static void freq_mhz_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    visit_type_int64(v, name, &m->freq_mhz, errp);
}

static void freq_mhz_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    visit_type_int64(v, name, &m->freq_mhz, errp);
}

static void num_irq_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    visit_type_int64(v, name, &m->num_irq, errp);
}

static void num_irq_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    visit_type_int64(v, name, &m->num_irq, errp);
}

static void flash_size_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    visit_type_uint64(v, name, &m->flash_size_kb, errp);
}

static void flash_size_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    visit_type_uint64(v, name, &m->flash_size_kb, errp);
}


static void virt_cortex_r_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Virtual ARM Cortex-R";
    mc->init = virt_cortex_r_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-r5f");

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
        oc, "flash-kb", "int",
        &flash_size_get, &flash_size_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "flash-kb",
        "Flash size in KB"
    );

}

static void virt_cortex_r_instance_init(Object* obj)
{
    VirtCortexRMachineState* m = VIRT_CORTEX_R_MACHINE(obj);
    m->freq_mhz = 50;
    m->num_irq = 64;
    m->flash_size_kb = 32 * 1024;
}

static const TypeInfo virt_cortex_r_type = {
    .name = TYPE_VIRT_CORTEX_R_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(VirtCortexRMachineState),
    .class_init = virt_cortex_r_class_init,
    .instance_init = virt_cortex_r_instance_init,
};


static void VIRT_CORTEX_R_MACHINE_init(void)
{
    type_register_static(&virt_cortex_r_type);
}

type_init(VIRT_CORTEX_R_MACHINE_init)