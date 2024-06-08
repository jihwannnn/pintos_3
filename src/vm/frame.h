#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/synch.h"
#include "vm/swap.h"

/* Frame table entry. */
struct frame_table_entry 
{
  void *kpage;
  struct hash_elem helem;
  struct list_elem lelem;
  void *upage;
  struct thread *t;
  bool pinned;
};


void vm_frame_init(void);
void vm_frame_free(void *kpage);
void *vm_frame_allocate(int flags, void *upage);
void vm_frame_do_free(void *kpage, bool free_page);
struct frame_table_entry *pick_frame_to_evict(uint32_t *pagedir);
void vm_frame_set_pinned(void *kpage, bool new_value);

#endif /* VM_FRAME_H */
