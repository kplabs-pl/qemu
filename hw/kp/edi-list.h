#ifndef EDI_LIST_H
#define EDI_LIST_H

#include "hw/kp/edi.h"

void kp_edi_destroy_message_chunk(kp_edi_message_chunk* chunk);
kp_edi_message_chunk* kp_edi_create_message_chunk(void);

kp_edi_message_chunk* kp_edi_chunk_list_pop_front(kp_edi_chunk_list* list);
void kp_edi_chunk_list_push_front(kp_edi_chunk_list* list, kp_edi_message_chunk* chunk);
void kp_edi_chunk_list_destroy(kp_edi_chunk_list* list);
void kp_edi_chunk_list_init(kp_edi_chunk_list* list);
void kp_edi_chunk_list_push_back(kp_edi_chunk_list* list, kp_edi_message_chunk* chunk);
bool kp_edi_chunk_list_empty(kp_edi_chunk_list* list);
kp_edi_message_chunk* kp_edi_chunk_list_front(kp_edi_chunk_list* list);
void kp_edi_chunk_list_clear(kp_edi_chunk_list* list);

#endif