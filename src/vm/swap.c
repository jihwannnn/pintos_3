#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include <bitmap.h>

static struct block *swap_block;
static struct bitmap *swap_bitmap;
static size_t swap_size;
static struct lock swap_lock;

void
vm_swap_init(void) {
  swap_block = block_get_role(BLOCK_SWAP);
  if (swap_block == NULL)
    PANIC("No swap block found!");

  swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
  swap_bitmap = bitmap_create(swap_size);
  if (swap_bitmap == NULL)
    PANIC("Failed to create swap bitmap!");

  bitmap_set_all(swap_bitmap, true);
  lock_init(&swap_lock);
}

swap_index_t
vm_swap_out(void *page) {
  lock_acquire(&swap_lock);

  size_t i;

  size_t swap_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, true);
  if (swap_index == BITMAP_ERROR)
    PANIC("Swap space full!");

  for (i = 0; i < SECTORS_PER_PAGE; i++) {
    block_write(swap_block, swap_index * SECTORS_PER_PAGE + i, page + i * BLOCK_SECTOR_SIZE);
  }

  lock_release(&swap_lock);
  return swap_index;
}

void
vm_swap_in(swap_index_t swap_index, void *page) {
  lock_acquire(&swap_lock);

  size_t i;

  ASSERT(bitmap_test(swap_bitmap, swap_index) == false);

  for (i = 0; i < SECTORS_PER_PAGE; i++) {
    block_read(swap_block, swap_index * SECTORS_PER_PAGE + i, page + i * BLOCK_SECTOR_SIZE);
  }

  bitmap_set(swap_bitmap, swap_index, true);

  lock_release(&swap_lock);
}
