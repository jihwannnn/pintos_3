#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include <bitmap.h>
#include "threads/synch.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

typedef size_t swap_index_t;


void vm_swap_init(void);
swap_index_t vm_swap_out(void *page);
void vm_swap_in(swap_index_t swap_index, void *page);

#endif /* VM_SWAP_H */
