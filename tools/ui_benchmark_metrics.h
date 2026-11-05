#pragma once
/* Diagnostic-only allocator observer. Fixed metadata, no observer allocations.
 * GNU ld wrappers see statically linked calls, not allocations inside libc. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static void bench_check(bool ok,const char *condition,const char *file,int line) {
  if (!ok) { fprintf(stderr,"editor benchmark contract failed: %s (%s:%d)\n",condition,file,line); exit(1); }
}
#define bench_require(condition) bench_check((condition),#condition,__FILE__,__LINE__)
typedef struct { void *pointer; size_t bytes; unsigned state; } BenchAllocation;
static BenchAllocation bench_heap[8192];
typedef struct { size_t requests, requested_bytes, frees, live, peak, unknown_frees; } BenchMetrics;
/* Link-time wrapping is invisible to compiler assumptions about libc's leaf
 * allocator declarations; force the observer to read the actual counters. */
static volatile BenchMetrics bench_metrics;
static size_t bench_slot(void *pointer,bool insert) {
  size_t at=((uintptr_t)pointer>>4)%8192, tomb=8192;
  for (size_t n=0;n<8192;++n,at=(at+1)%8192) {
    if (bench_heap[at].state==1 && bench_heap[at].pointer==pointer) return at;
    if (bench_heap[at].state==2 && tomb==8192) tomb=at;
    if (!bench_heap[at].state) return insert?(tomb<8192?tomb:at):8192;
  }
  return insert?tomb:8192;
}
static void bench_remove(size_t at) {
  if (at==8192) { ++bench_metrics.unknown_frees; return; }
  bench_metrics.live-=bench_heap[at].bytes;
  bench_heap[at]=(BenchAllocation){.state=2};
}
static void bench_insert(void *pointer,size_t bytes) {
  if (!pointer) return;
  size_t at=bench_slot(pointer,true); bench_require(at<8192);
  bench_heap[at]=(BenchAllocation){pointer,bytes,1};
  bench_metrics.live+=bytes;
  if (bench_metrics.live>bench_metrics.peak) bench_metrics.peak=bench_metrics.live;
}
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void __real_free(void *);
void *__wrap_malloc(size_t bytes) {
  ++bench_metrics.requests; bench_metrics.requested_bytes+=bytes;
  void *p=__real_malloc(bytes); bench_insert(p,bytes); return p;
}
void *__wrap_calloc(size_t n,size_t bytes) {
  ++bench_metrics.requests; bench_metrics.requested_bytes+=n*bytes;
  void *p=__real_calloc(n,bytes); bench_insert(p,n*bytes); return p;
}
void *__wrap_realloc(void *pointer,size_t bytes) {
  ++bench_metrics.requests; bench_metrics.requested_bytes+=bytes;
  size_t at=pointer?bench_slot(pointer,false):8192;
  void *next=__real_realloc(pointer,bytes);
  if (next || !bytes) {
    if (pointer) bench_remove(at);
    bench_insert(next,bytes);
  }
  return next;
}
void __wrap_free(void *pointer) {
  if (pointer) { ++bench_metrics.frees; bench_remove(bench_slot(pointer,false)); }
  __real_free(pointer);
}
static void bench_reset_metrics(void) {
  size_t live=bench_metrics.live;
  bench_metrics=(BenchMetrics){.live=live,.peak=live};
}
static inline double bench_now(void) {
  struct timespec t; bench_require(clock_gettime(CLOCK_MONOTONIC,&t)==0);
  return t.tv_sec+t.tv_nsec*1e-9;
}
static inline int bench_compare(const void *a,const void *b) {
  double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y);
}
