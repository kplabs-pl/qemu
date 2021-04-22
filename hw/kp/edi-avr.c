#include "hw/kp/edi.h"
#include "hw/avr/atmega.h"
#include "target/avr/cpu.h"
#include "hw/irq.h"

static void trigger_irq(KPEDIState* s)
{
    if(s->irq != NULL) 
    {
        qemu_set_irq(s->irq, 1);
    }
}

static void set_irq(KPEDIState* s)
{
    if (s->registers.interrupt == 0xff)
    {
        s->irq = NULL;
        return;
    }

    s->irq = qdev_get_gpio_in(DEVICE(first_cpu), s->registers.interrupt - 1);
}

void kp_edi_init_arch(KPEDIState *s)
{
    s->registers.id = 0xBE;
    s->trigger_irq = trigger_irq;
    s->set_irq = set_irq;
    s->register_ops = &KpEdiRegisterOpsReg8Ptr16;
    s->address_space_size = KP_EDI_ADDRESS_SPACE_SIZE_REG8_PTR16;
}
