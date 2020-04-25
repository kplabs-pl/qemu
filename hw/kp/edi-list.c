#include "edi-list.h"
#include <nanomsg/nn.h>

static void init_msg_chunk(kp_edi_message_chunk* chunk)
{
    chunk->message = NULL;
    chunk->size = 0;
}

static void cleanup_msg_chunk(kp_edi_message_chunk* chunk)
{
    if(chunk->message != NULL)
    {
        nn_freemsg(chunk->message);
    }
}

kp_edi_message_chunk* kp_edi_create_message_chunk(void)
{
    kp_edi_message_chunk* chunk = g_malloc(sizeof(kp_edi_message_chunk));
    init_msg_chunk(chunk);
    return chunk;
}

void kp_edi_destroy_message_chunk(kp_edi_message_chunk* chunk)
{
    cleanup_msg_chunk(chunk);
    g_free(chunk);
}

void kp_edi_chunk_list_init(kp_edi_chunk_list* list)
{
    list->size = 0;
    QSIMPLEQ_INIT(&list->head);
}

void kp_edi_chunk_list_clear(kp_edi_chunk_list* list)
{
    kp_edi_message_chunk* chunk = list->head.sqh_first;
    while(chunk != NULL)
    {
        kp_edi_message_chunk* next = chunk->list_entry.sqe_next;
        kp_edi_destroy_message_chunk(chunk);
        chunk = next;
    }

    kp_edi_chunk_list_init(list);
}

void kp_edi_chunk_list_destroy(kp_edi_chunk_list* list)
{
    kp_edi_chunk_list_clear(list);
}

void kp_edi_chunk_list_push_back(kp_edi_chunk_list* list, kp_edi_message_chunk* chunk)
{
    QSIMPLEQ_INSERT_TAIL(&list->head, chunk, list_entry);
    ++list->size;
}

void kp_edi_chunk_list_push_front(kp_edi_chunk_list* list, kp_edi_message_chunk* chunk)
{
    QSIMPLEQ_INSERT_HEAD(&list->head, chunk, list_entry);
    ++list->size;
}

bool kp_edi_chunk_list_empty(kp_edi_chunk_list* list)
{
    return QSIMPLEQ_EMPTY(&list->head);
}

kp_edi_message_chunk* kp_edi_chunk_list_front(kp_edi_chunk_list* list)
{
    if(kp_edi_chunk_list_empty(list))
    {
        return NULL;
    }

    return  QSIMPLEQ_FIRST(&list->head);
}

kp_edi_message_chunk* kp_edi_chunk_list_pop_front(kp_edi_chunk_list* list)
{
    if(kp_edi_chunk_list_empty(list))
    {
        return NULL;
    }

    kp_edi_message_chunk* chunk = QSIMPLEQ_FIRST(&list->head);
    QSIMPLEQ_REMOVE_HEAD(&list->head, list_entry);
    --list->size;
    return chunk;
}