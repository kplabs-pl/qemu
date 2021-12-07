#include "hw/kp/edi.h"
#include "hw/irq.h"
#include "qapi/error.h"

static void trigger_irq(KPEDIState* s)
{
    if(s->irq != NULL)
    {
        qemu_irq_pulse(s->irq);
    }
}

static void set_irq(KPEDIState* s)
{
    if(s->registers.interrupt == UINT32_MAX)
    {
        s->irq = NULL;
    }
    else
    {
        Object* machine = qdev_get_machine();
        Object* intc = object_property_get_link(machine, "intc_device", &error_abort);
        qemu_irq irq =  qdev_get_gpio_in(DEVICE(intc), s->registers.interrupt);
        s->irq = irq;
    }
}

void kp_edi_init_arch(KPEDIState *s)
{
    s->registers.id = 0xDEADCAFE;
    s->trigger_irq = trigger_irq;
    s->set_irq = set_irq;
    s->register_ops = &KpEdiRegisterOpsReg32Ptr32;
    s->address_space_size = KP_EDI_ADDRESS_SPACE_SIZE_REG32_PTR32;
}
