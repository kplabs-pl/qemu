#include "hw/kp/edi.h"
#include "trace/trace-hw_kp.h"
#include "edi-commands.h"

static uint64_t kp_edi_register_read_reg8_ptr16(void *opaque, hwaddr addr, unsigned size)
{
    KPEDIState *s = (KPEDIState*)opaque;

    trace_kp_edi_reg_read(s->name, addr);

    switch(addr)
    {
        case 0x00:
            return s->registers.command;
        case 0x01:
            return s->registers.pointer & 0xFF;
        case 0x02:
            return (s->registers.pointer >> 8) & 0xFF;
        case 0x03:
            return s->registers.size;
        case 0x04:
            return s->registers.interrupt & 0xFF;
        case 0x05:
            return s->registers.id;
        default:
            trace_kp_edi_error_reg_read(s->name, addr);
            return 0xBE;
    }
}

static void kp_edi_register_write_reg8_ptr16(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    KPEDIState *s = (KPEDIState*)opaque;

    trace_kp_edi_reg_write(s->name, addr, data);

    switch(addr)
    {
        case 0x00:
            s->registers.command = kp_edi_handle_command(s, data);
            break;
        case 0x01:
            s->registers.pointer &= 0x0000FF00;
            s->registers.pointer |= 0x00800000;
            s->registers.pointer |= (uint16_t)(data & 0xFF);
            break;
        case 0x02:
            s->registers.pointer &= 0x000000FF;
            s->registers.pointer |= 0x00800000;
            s->registers.pointer |= (uint16_t)((data & 0xFF) << 8);
            break;
        case 0x03:
            s->registers.size = (uint8_t)data;
            break;
        case 0x04:
            s->registers.interrupt = (uint8_t)data;
            s->set_irq(s);
            break;
        default:
            trace_kp_edi_error_reg_write(s->name, addr, data);
            break;
    }
}

static uint64_t kp_edi_register_read_reg32_ptr32(void *opaque, hwaddr addr, unsigned size)
{
    KPEDIState *s = (KPEDIState*)opaque;

    trace_kp_edi_reg_read(s->name, addr);

    switch(addr)
    {
        case 0x00:
            return s->registers.command;
        case 0x04:
            return s->registers.pointer;
        case 0x08:
            return s->registers.size;
        case 0x0C:
            return s->registers.interrupt;
        case 0x10:
            return s->registers.id;
        default:
            trace_kp_edi_error_reg_read(s->name, addr);
            return 0xDEADBEEF;
    }
}

static void kp_edi_register_write_reg32_ptr32(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    KPEDIState *s = (KPEDIState*)opaque;

    trace_kp_edi_reg_write(s->name, addr, data);

    switch(addr)
    {
        case 0x00:
            s->registers.command = kp_edi_handle_command(s, data);
            break;
        case 0x04:
            s->registers.pointer = (uint32_t)data;
            break;
        case 0x08:
            s->registers.size = (uint32_t)data;
            break;
        case 0x0C:
            s->registers.interrupt = (uint32_t)data;
            s->set_irq(s);
            break;
        default:
            trace_kp_edi_error_reg_write(s->name, addr, data);
            break;
    }
}

const MemoryRegionOps KpEdiRegisterOpsReg8Ptr16 = {
    .read = kp_edi_register_read_reg8_ptr16,
    .write = kp_edi_register_write_reg8_ptr16,
};

const MemoryRegionOps KpEdiRegisterOpsReg32Ptr32 = {
    .read = kp_edi_register_read_reg32_ptr32,
    .write = kp_edi_register_write_reg32_ptr32,
};
