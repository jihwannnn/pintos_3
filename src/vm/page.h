#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/off_t.h"
#include "vm/swap.h"

enum page_status{
  ALL_ZERO,
  ON_FRAME,
  ON_SWAP,
  FROM_FILESYS
};

/* Supplemental page table entry. */
struct supplemental_page_table_entry {
  void *upage;
  void *kpage;
  struct hash_elem elem;
  enum page_status status;
  swap_index_t swap_index;
  bool dirty;
  struct file *file;
  off_t file_offset;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};

/* Supplemental page table. */
struct supplemental_page_table {
  struct hash page_map;
};

struct supplemental_page_table_entry *vm_supt_lookup(struct supplemental_page_table *supt, void *page);
bool vm_supt_install_filesys(struct supplemental_page_table *supt, void *upage, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool vm_supt_install_frame(struct supplemental_page_table *supt, void *upage, void *kpage);
bool vm_supt_set_swap(struct supplemental_page_table *supt, void *page, swap_index_t swap_index);
bool vm_supt_set_dirty(struct supplemental_page_table *supt, void *page, bool value);
bool vm_supt_mm_unmap(struct supplemental_page_table *supt, uint32_t *pagedir, void *page, struct file *f, off_t offset, size_t bytes);
bool vm_supt_install_zeropage(struct supplemental_page_table *supt, void *upage);
void preload_and_pin_pages(const void *buffer, size_t size);
void unpin_preloaded_pages(const void *buffer, size_t size);
bool vm_load_page (struct supplemental_page_table *supt, uint32_t *pagedir, void *upage);

#endif /* VM_PAGE_H */
