#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include <stdbool.h>
#include <stdint.h>

/* Frame table and lock */
static struct hash frame_map;
static struct lock frame_lock;

void *vm_frame_allocate(enum palloc_flags flags, void *upage);

/* Hash function for frame table */
unsigned
frame_hash(const struct hash_elem *f_, void *aux UNUSED) {
  const struct frame_table_entry *f = hash_entry(f_, struct frame_table_entry, helem);
  return hash_bytes(&f->kpage, sizeof f->kpage);
}

/* Less function for frame table */
bool
frame_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct frame_table_entry *a = hash_entry(a_, struct frame_table_entry, helem);
  const struct frame_table_entry *b = hash_entry(b_, struct frame_table_entry, helem);
  return a->kpage < b->kpage;
}

/* Initializes the frame table */
void
vm_frame_init(void) {
  lock_init(&frame_lock);
  hash_init(&frame_map, frame_hash, frame_less, NULL);
}

/* Allocates a frame */
void *
vm_frame_allocate(enum palloc_flags flags, void *upage) {
  lock_acquire(&frame_lock);
  void *kpage = palloc_get_page(PAL_USER | flags);
  if (kpage == NULL) {
    struct frame_table_entry *fte = pick_frame_to_evict(thread_current()->pagedir);
    if (fte != NULL) {
      swap_index_t swap_index = vm_swap_out(fte->kpage);
      vm_supt_set_swap(fte->t->supt, fte->upage, swap_index);
      pagedir_clear_page(fte->t->pagedir, fte->upage);
      vm_frame_do_free(fte->kpage, true);
      kpage = palloc_get_page(PAL_USER | flags);
    }
  }

  if (kpage != NULL) {
    struct frame_table_entry *fte = malloc(sizeof *fte);
    if (fte == NULL) {
      palloc_free_page(kpage);
      lock_release(&frame_lock);
      return NULL;
    }
    fte->kpage = kpage;
    fte->upage = upage;
    fte->t = thread_current();
    fte->pinned = false;
    hash_insert(&frame_map, &fte->helem);
  }
  lock_release(&frame_lock);
  return kpage;
}

/* Frees a frame */
void
vm_frame_free(void *kpage) {
  lock_acquire(&frame_lock);
  vm_frame_do_free(kpage, true);
  lock_release(&frame_lock);
}

/* Does the actual freeing of the frame */
void
vm_frame_do_free(void *kpage, bool free_page) {
  struct frame_table_entry fte;
  struct hash_elem *e;

  fte.kpage = kpage;
  e = hash_find(&frame_map, &fte.helem);
  if (e != NULL) {
    struct frame_table_entry *fte = hash_entry(e, struct frame_table_entry, helem);
    hash_delete(&frame_map, e);
    free(fte);
    if (free_page) {
      palloc_free_page(kpage);
    }
  }
}

/* Picks a frame to evict using the clock algorithm */
struct frame_table_entry*
pick_frame_to_evict(uint32_t *pagedir) {
  struct hash_iterator i;
  hash_first(&i, &frame_map);
  while (hash_next(&i)) {
    struct frame_table_entry *fte = hash_entry(hash_cur(&i), struct frame_table_entry, helem);
    if (!fte->pinned && !pagedir_is_accessed(pagedir, fte->upage)) {
      return fte;
    }
    pagedir_set_accessed(pagedir, fte->upage, false);
  }
  return NULL;
}

/* Sets the pin state of a frame */
void
vm_frame_set_pinned(void *kpage, bool new_value) {
  lock_acquire(&frame_lock);
  struct frame_table_entry fte;
  struct hash_elem *e;

  fte.kpage = kpage;
  e = hash_find(&frame_map, &fte.helem);
  if (e != NULL) {
    struct frame_table_entry *fte = hash_entry(e, struct frame_table_entry, helem);
    fte->pinned = new_value;
  }
  lock_release(&frame_lock);
}
