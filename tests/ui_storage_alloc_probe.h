#pragma once
/* Observe only realloc-owned private buffers; context malloc and text history
 * allocations are outside this probe. No libc allocator metadata is changed. */
static size_t storage_requests,storage_live;
static int storage_fail_after=-1;
static bool storage_force_move;
typedef struct { void *pointer; size_t bytes; } StorageAllocation;
static StorageAllocation storage_owned[64];
void *__real_realloc(void *,size_t);
void __real_free(void *);
void *__wrap_realloc(void *pointer,size_t bytes) {
  ++storage_requests;
  if(storage_fail_after>=0&&storage_fail_after--==0) return NULL;
  size_t slot=64;
  for(size_t i=0;i<64;++i) if(pointer?storage_owned[i].pointer==pointer:!storage_owned[i].pointer) { slot=i; break; }
  CHECK(slot<64);
  size_t previous=storage_owned[slot].bytes;
  void *next;
  if(pointer&&storage_force_move) {
    next=malloc(bytes);
    if(next) {
      memcpy(next,pointer,previous<bytes?previous:bytes); __real_free(pointer);
      storage_force_move=false;
    }
  } else next=__real_realloc(pointer,bytes);
  if(next) { storage_owned[slot]=(StorageAllocation){next,bytes}; storage_live=storage_live-previous+bytes; }
  return next;
}
void __wrap_free(void *pointer) {
  if(pointer) for(size_t i=0;i<64;++i) if(storage_owned[i].pointer==pointer) {
    storage_live-=storage_owned[i].bytes; storage_owned[i]=(StorageAllocation){0}; break;
  }
  __real_free(pointer);
}
