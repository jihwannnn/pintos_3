#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <hash.h>
#include <stdbool.h>
#include <stdint.h>

/* Hash function for the supplemental page table */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct supplemental_page_table_entry *p = hash_entry (p_, struct supplemental_page_table_entry, elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

/* Less function for the supplemental page table */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct supplemental_page_table_entry *a = hash_entry (a_, struct supplemental_page_table_entry, elem);
  const struct supplemental_page_table_entry *b = hash_entry (b_, struct supplemental_page_table_entry, elem);
  return a->upage < b->upage;
}

/* Looks up a page in the supplemental page table */
struct supplemental_page_table_entry *
vm_supt_lookup(struct supplemental_page_table *supt, void *page) {
  struct supplemental_page_table_entry p;
  struct hash_elem *e;

  p.upage = page;
  e = hash_find(&supt->page_map, &p.elem);
  return e != NULL ? hash_entry(e, struct supplemental_page_table_entry, elem) : NULL;
}

/* Installs a file-backed page into the supplemental page table */
bool
vm_supt_install_filesys(struct supplemental_page_table *supt, void *upage, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
  struct supplemental_page_table_entry *spte = malloc(sizeof *spte);
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = FROM_FILESYS;
  spte->file = file;
  spte->file_offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->dirty = false;
  spte->swap_index = -1;

  struct hash_elem *prev = hash_insert(&supt->page_map, &spte->elem);
  if (prev != NULL) {
    free(spte);
    return false;
  }
  return true;
}

/* Installs a frame-backed page into the supplemental page table */
bool
vm_supt_install_frame(struct supplemental_page_table *supt, void *upage, void *kpage) {
  struct supplemental_page_table_entry *spte = malloc(sizeof *spte);
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->kpage = kpage;
  spte->status = ON_FRAME;
  spte->dirty = false;
  spte->swap_index = -1;

  struct hash_elem *prev = hash_insert(&supt->page_map, &spte->elem);
  if (prev != NULL) {
    free(spte);
    return false;
  }
  return true;
}

/* Sets the swap status for a page in the supplemental page table */
bool
vm_supt_set_swap(struct supplemental_page_table *supt, void *page, swap_index_t swap_index) {
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
  if (spte == NULL)
    return false;

  spte->kpage = NULL;
  spte->status = ON_SWAP;
  spte->swap_index = swap_index;
  return true;
}

/* Sets the dirty status for a page in the supplemental page table */
bool
vm_supt_set_dirty(struct supplemental_page_table *supt, void *page, bool value) {
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
  if (spte == NULL)
    return false;

  spte->dirty = value;
  return true;
}

/* Unmaps a page from the supplemental page table */
bool
vm_supt_mm_unmap(struct supplemental_page_table *supt, uint32_t *pagedir, void *page, struct file *f, off_t offset, size_t bytes) {
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
  if (spte == NULL)
    return false;

  if (spte->status == ON_FRAME) {
    if (pagedir_is_dirty(pagedir, spte->upage) || spte->dirty) {
      file_write_at(f, spte->upage, bytes, offset);
    }
    pagedir_clear_page(pagedir, spte->upage);
    palloc_free_page(spte->kpage);
  } else if (spte->status == ON_SWAP) {
    if (spte->dirty) {
      // Implement swap-in and then write to file
    }
    // Free swap slot
  }
  hash_delete(&supt->page_map, &spte->elem);
  free(spte);
  return true;
}

/* Installs a zero-initialized page into the supplemental page table */
bool
vm_supt_install_zeropage(struct supplemental_page_table *supt, void *upage) {
  struct supplemental_page_table_entry *spte = malloc(sizeof *spte);
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = ALL_ZERO;
  spte->dirty = false;
  spte->swap_index = -1;

  struct hash_elem *prev = hash_insert(&supt->page_map, &spte->elem);
  if (prev != NULL) {
    free(spte);
    return false;
  }
  return true;
}

/* Preloads and pins pages */
void
preload_and_pin_pages(const void *buffer, size_t size) {
  struct supplemental_page_table *supt = thread_current()->supt;
  uint32_t *pagedir = thread_current()->pagedir;
  void *upage;
  for (upage = pg_round_down(buffer); upage < buffer + size; upage += PGSIZE) {
    vm_load_page(supt, pagedir, upage);
    vm_frame_set_pinned(upage, true);
  }
}

/* Unpins preloaded pages */
void
unpin_preloaded_pages(const void *buffer, size_t size) {
  struct supplemental_page_table *supt = thread_current()->supt;
  void *upage;
  for (upage = pg_round_down(buffer); upage < buffer + size; upage += PGSIZE) {
    vm_frame_set_pinned(upage, false);
  }
}

bool
vm_load_page (struct supplemental_page_table *supt, uint32_t *pagedir, void *upage) 
{
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, upage);
  if (spte == NULL) 
  {
    return false;
  }

  // Obtain a new frame for the page
  void *kpage = vm_frame_allocate(PAL_USER, upage);
  if (kpage == NULL) 
  {
    return false;
  }

  switch (spte->status) 
  {
    case ON_SWAP:
      // Load the page from swap space
      vm_swap_in(spte->swap_index, kpage);
      break;

    case FROM_FILESYS:
      // Load the page from the file system
      if (file_read_at(spte->file, kpage, spte->read_bytes, spte->file_offset) != (int) spte->read_bytes) 
      {
        vm_frame_free(kpage);
        return false;
      }
      memset(kpage + spte->read_bytes, 0, PGSIZE - spte->read_bytes);
      break;

    case ALL_ZERO:
      // Load a zeroed page
      memset(kpage, 0, PGSIZE);
      break;

    default:
      vm_frame_free(kpage);
      return false;
  }

  if (!pagedir_set_page(pagedir, upage, kpage, spte->writable)) 
  {
    vm_frame_free(kpage);
    return false;
  }

  spte->kpage = kpage;
  spte->status = ON_FRAME;
  return true;
}
