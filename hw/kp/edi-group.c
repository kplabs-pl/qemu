#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/kp/edi.h"
#include "qapi/error.h"

typedef struct KPEDIGroupState {
    DeviceState parent_obj;

    uint64_t addr;
    uint8_t count;
    char* name;

    KPEDIState* children;
} KPEDIGroupState;

#define TYPE_KP_EDI_GROUP "kp-edi-group"
#define KP_EDI_GROUP(obj) OBJECT_CHECK(KPEDIGroupState, (obj), \
                                         TYPE_KP_EDI_GROUP)

static void kp_edi_group_init(Object* obj)
{
    KPEDIGroupState *s = KP_EDI_GROUP(obj);
    s->children = NULL;
}

static void kp_edi_group_realize(DeviceState *dev, Error **errp)
{
    KPEDIGroupState *s = KP_EDI_GROUP(dev);
    s->children = g_new(KPEDIState, s->count);

    const char* nameFormat = s->name;

    if (nameFormat == NULL)
    {
        nameFormat = "EDI%02d";
    }

    for(uint8_t i = 0; i < s->count; i++)
    {
        gchar *name = g_strdup_printf("edi[%u]", i);
        gchar *logName = g_strdup_printf(nameFormat, i);

        object_initialize_child(OBJECT(s), name, &s->children[i], TYPE_KP_EDI);

        object_property_set_int(OBJECT(&s->children[i]), "addr", s->addr + KP_EDI_ADDRESS_SPACE_SIZE * i, &error_abort);
        object_property_set_str(OBJECT(&s->children[i]), "name", logName, &error_abort);
        
        g_free(logName);
        g_free(name);
    }

    for(uint8_t i = 0; i < s->count; i++)
    {
        object_property_set_bool(OBJECT(&s->children[i]), "realized", true, errp);
    }
}

static void kp_edi_group_unrealize(DeviceState *dev)
{ 
    KPEDIGroupState *s = KP_EDI_GROUP(dev);
    g_free(s->children);
}

static Property kp_edi_group_props[] = {
    DEFINE_PROP_UINT64("addr", KPEDIGroupState, addr, 0xF0000010),
    DEFINE_PROP_UINT8("count", KPEDIGroupState, count, 20),
    DEFINE_PROP_STRING("name", KPEDIGroupState, name),
    DEFINE_PROP_END_OF_LIST(),
};

static void kp_edi_group_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = kp_edi_group_realize;
    dc->unrealize = kp_edi_group_unrealize;

    device_class_set_props(dc, kp_edi_group_props);

    dc->desc = "KP Labs - External Device Interface Group";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_edi_group_info = {
    .name = TYPE_KP_EDI_GROUP,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPEDIGroupState),
    .class_init = kp_edi_group_class_init,
    .instance_init = kp_edi_group_init
};

static void kp_edi_group_register_type(void)
{
    type_register_static(&kp_edi_group_info);
}

type_init(kp_edi_group_register_type)