#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

#include <cstddef>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include <x86intrin.h>

#include<set>
#include<map>


extern bool enableInstrument;
extern long unsigned histGran[8];
extern long unsigned histMedTaskLen[8];
extern long unsigned histIteration[8];

extern "C"{
extern void resume2scheduler_eager(void** ctx, int fromSync);
extern char sync_eagerd(void** ctx, int owner, void* savedRSP, void* currentRSP);
extern void** allocate_parallelctx2(void**ctx);
extern void set_joincntr(void**ctx);
extern void deallocate_parallelctx(void** ctx);

extern void push_workctx_eager(void** ctx, int* bWorkNotPush);
extern void pop_workctx_eager(void** ctx, int* bWorkNotPush);
extern __thread int threadId;

extern void suspend2scheduler_shared(void** resumeCtx);
extern void resume2scheduler(void** resumeCtx, void* newsp);
extern void push_workmsg(void** parallelCtx, int owner);

extern void **get_stacklet_ctx();

extern __thread int initDone;
}

#define getSP(sp) asm volatile("#getsp\n\tmovq %%rsp,%[Var]" : [Var] "=r" (sp))

#define callerTrashed() asm volatile("# all caller saved are trashed here" : : : "rdi", "rsi", "r8", "r9", "r10", "r11", "rdx", "rcx", "rax")
#define calleeTrashed() asm volatile("# all callee saved are trashed here" : : : "rbx", "r12", "r13", "r14", "r15", "rax")
#define intregTrashed() asm volatile("# all integer saved are trashed here" : : : "rbx", "r12", "r13", "r14", "r15", "rax", "rdi", "rsi", "r8", "r9", "r10", "r11", "rdx", "rcx", "xmm0")

extern std::map<long unsigned , std::set<long unsigned>> taskLen2Gran;
extern std::map<long unsigned , std::set<long unsigned>> taskLen2Iteration;

#define ss_fence() __asm__ volatile ("lock addl $0,(%rsp)")
namespace parlay {

 // IWYU pragma: private, include "../../parallel.h"

 inline size_t num_workers() { return __cilkrts_get_nworkers(); }
 inline size_t worker_id() { return __cilkrts_get_worker_number(); }

 template <typename F>
 __attribute__((noinline)) void wrapperF(F f) {
   f();
 }

 template <typename F>
 __attribute__((noinline)) void wrapperFi(F f, size_t i) {
   f(i);
 }

 template <typename Lf, typename Rf>
 inline void par_do(Lf left, Rf right, bool) {
   cilk_spawn wrapperF(right);
   wrapperF(left);
   cilk_sync;
   //cilk_spawn right();
   //left();
   //cilk_sync;
 }


template <typename F>
__attribute__((noinline)) void parallel_for_eager(size_t start, size_t end, F f, long granularity, bool conservative, int depth, int threshDepth);

template <typename F>
__attribute__((noinline))
__attribute__((no_unwind_path))
void parallel_for_eager_wrapper(size_t start, size_t end, F f, long granularity,
				bool conservative, int depth, int threshDepth, void** readyCtx) {
  int bWorkNotPush;
  push_workctx_eager(readyCtx, &bWorkNotPush);
  parallel_for_eager(start, end, f, granularity, true, depth, threshDepth);
  pop_workctx_eager(readyCtx, &bWorkNotPush);
  return;
}

 template <typename F>
   __attribute__((noinline)) void parallel_for_recurse(size_t start, size_t end, F f, long granularity, bool conservative) {

 tail_recurse:
   if ((end - start) <= static_cast<size_t>(granularity)) {
#if 1
     for (size_t i=start; i < end; i++) f(i);
#else
     size_t len = end - start;
     size_t len_g = len/8;
     for (size_t i=0; i < len_g; i++) {
       //f(start+granularity*i+j);
       f(start+8*i+0);
       f(start+8*i+1);
       f(start+8*i+2);
       f(start+8*i+3);
       f(start+8*i+4);
       f(start+8*i+5);
       f(start+8*i+6);
       f(start+8*i+7);
    }
    long start_rem = start + 8*(len_g);
    for(size_t i = start_rem; i<end; i++) {
      f(i);
    }
#endif
   } else {
     size_t n = end-start;
     size_t mid = (start + n/2);
     cilk_spawn parallel_for_recurse(start, mid, f, granularity, true);
     start = mid;
     goto tail_recurse;
   }

   cilk_sync;

 }

static int dummyfcn() {return 0;}

 template <typename F>
__attribute__((noinline))
__attribute__((no_unwind_path))
void parallel_for_eager(size_t start, size_t end, F f, long granularity,  bool conservative, int depth, int threshDepth) {

   if(end-start <= threshDepth) {
     //printf("[%d] depth: %d >= %d\n", threadId, depth, threshDepth);
     parallel_for_recurse(start, end, f, granularity, true);
     return;
   }

   int owner = threadId;
   void* readyCtx[64];
   void** ctx = allocate_parallelctx2((void**)readyCtx);
   getSP(readyCtx[2]);
   readyCtx[23] = NULL;
   readyCtx[21] = NULL;
   readyCtx[24] = (void*) -1;
   readyCtx[19] = (void*)owner;
   readyCtx[20] = (void*)&readyCtx[0];

   if ((end - start) <= static_cast<size_t>(granularity)) {
#if 1
     for (size_t i=start; i < end; i++) f(i);
#else
     size_t len = end - start;
     size_t len_g = len/8;
     for (size_t i=0; i < len_g; i++) {
       //f(start+granularity*i+j);
       f(start+8*i+0);
       f(start+8*i+1);
       f(start+8*i+2);
       f(start+8*i+3);
       f(start+8*i+4);
       f(start+8*i+5);
       f(start+8*i+6);
       f(start+8*i+7);
    }
    long start_rem = start + 8*(len_g);
    for(size_t i = start_rem; i<end; i++) {
      f(i);
    }
#endif
   }  else {
     size_t n = end-start;
     size_t mid = (start + n/2);

     __builtin_multiret_call(2, 1, (void*)&dummyfcn, (void*)readyCtx, &&det_cont, &&det_cont);
     parallel_for_eager_wrapper(start, mid, f, granularity, true, depth+1, threshDepth, (void**) &readyCtx[0]);
   det_cont:{
       //intregTrashed();
       parallel_for_eager(mid, end, f, granularity, true, depth+1, threshDepth);
     }

     // Synchronize
     void* sp2;// = (void*)__builtin_read_sp();
     getSP(sp2);
     if(!sync_eagerd((void**)readyCtx, (int)owner, (void*)readyCtx[2], sp2)) {
       __builtin_multiret_call(2, 1, (void*)&dummyfcn, (void*)readyCtx, &&sync_pre_resume_parent, &&sync_pre_resume_parent);
       resume2scheduler_eager((void**)readyCtx, 1);
     sync_pre_resume_parent: {
	 //intregTrashed();
	 set_joincntr((void**)readyCtx);
       }
     }
     //gCntrAba[threadId].ptr++;
     deallocate_parallelctx((void**)readyCtx);
   }
   return;
 }

template <typename F>
__attribute__((noinline))
void parallel_for_seq(size_t start, size_t end, F f,
		       long granularity,
		       bool conservative) {

  size_t len = end - start;
  size_t len_g = len/granularity;
  if (len_g > 1) {
    cilk_for (size_t i=0; i < len_g; i++) {
      for (size_t j=0; j<granularity; j++) {
	f(start+granularity*i+j);
      }
    }
    long start_rem = start + granularity*(len_g);
    for(size_t i = start_rem; i<end; i++) {
      f(i);
    }
  } else {
    cilk_for (size_t i=start; i < end; i++) {
      f(i);
    }
  }
}

static __thread int delegate_work = 0;

static  size_t start_par[72] ;
static  size_t end_par[72] ;
static  long granularity_par[72] ;

template <typename F>
__attribute__((noinline))
__attribute__((no_unwind_path))
void parallel_for_static(size_t start, size_t end, F f, long granularity, bool conservative, void** resumeCtx) {
  size_t size = end - start;
  size_t nWorkers = num_workers();
  size_t static_range = size/nWorkers;

  void* parallelCtx[64];
  getSP(resumeCtx[2]);
#if 1
  __builtin_multiret_call(2, 1, (void*)&dummyfcn, (void*)parallelCtx, &&det_cont_static, &&det_cont_static);
#else
  volatile int dummy_a = 0;
  asm volatile("movl $0, %[ARG]\n\t":[ARG] "=r" (dummy_a));
  __builtin_uli_save_context((void*)parallelCtx, &&det_cont_static_before);
  if(dummy_a){
  det_cont_static_before:
    goto det_cont_static;
  }
#endif

  for(int i=1; i<nWorkers; i++) {
    start_par[i] = start + i*static_range;
    end_par[i] = start + (i+1)*(static_range);
    if(i == nWorkers-1)
      end_par[i] = end;
    granularity_par[i] = granularity;
    // Send the message
    push_workmsg((void**)parallelCtx, i);
  }
  //ss_fence();
  //printf("[%d]Start range static: %zu %zu\n", threadId, start, start+static_range);
  //parallel_for_recurse(start, start+static_range, f, granularity, true);
  parallel_for_eager(start, start+static_range, f, granularity, true, 0, static_range/num_workers());
  //for (size_t i=start; i < start+static_range; i++) f(i);
  //printf("[%d]End range static: %zu %zu\n", threadId, start, start+static_range);

  if(resumeCtx[17] > (void*)1) {
    //assert(resumeCtx[17] != 0 && "Synchronizer can not be 0 here");
    //getSP(resumeCtx[2]);
#if 1
    __builtin_multiret_call(2, 1, (void*)&dummyfcn, (void*)resumeCtx, &&sync_pre_resume_parent_static, &&sync_pre_resume_parent_static);
#else
    __builtin_uli_save_context((void*)resumeCtx, &&sync_pre_resume_parent_static_before);
    if(dummy_a) {
    sync_pre_resume_parent_static_before:
      goto sync_pre_resume_parent_static;
    }
#endif
    suspend2scheduler_shared((void**)resumeCtx);
  sync_pre_resume_parent_static: {
      //calleeTrashed();
      //callerTrashed();
    }
  }
  return;

  det_cont_static: {
    //calleeTrashed();
    //callerTrashed();
    size_t start_par_l = start + threadId*static_range;
    size_t end_par_l = start + (threadId+1)*(static_range);
    if(threadId == nWorkers-1)
      end_par_l = end;

    //printf("[%d]Start range static: [%zu + %zu] %zu %zu granularity:%d\n", threadId, start, threadId*static_range, start_par_l, end_par_l, granularity_par[threadId]);
    //parallel_for_recurse(start_par[threadId], end_par[threadId], f, granularity_par[threadId], true);
    parallel_for_eager(start_par[threadId], end_par[threadId], f, granularity_par[threadId], true, 0, (end_par[threadId]-start_par[threadId]) / num_workers() );
    //for (size_t i=start_par[threadId]; i < end_par[threadId]; i++) f(i);
    //for (size_t i=start_par_l; i < end_par_l; i++) f(i);
    // printf("[%d]End range static: %zu %zu\n", threadId, start_par[threadId], end_par[threadId]);
    resume2scheduler((void**)resumeCtx, get_stacklet_ctx()[18]);
  }
  return;
 }


template <typename F>
//__attribute__((noinline)) void parallel_for(size_t start, size_t end, F f,
inline void parallel_for(size_t start, size_t end, F f,
                         long granularity,
                         bool ) {
#if 0
  // Default
  if (granularity == 0) {
    cilk_for(size_t i=start; i<end; i++) f(i);
  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }
#else
  // Experiment
  if ((end - start) <= static_cast<size_t>(granularity)) {
#if 1
    for (size_t i=start; i < end; i++) f(i);
#else
    size_t len = end - start;
    size_t len_g = len/8;
    for (size_t i=0; i < len_g; i++) {
      //f(start+granularity*i+j);
      f(start+8*i+0);
      f(start+8*i+1);
      f(start+8*i+2);
      f(start+8*i+3);
      f(start+8*i+4);
      f(start+8*i+5);
      f(start+8*i+6);
      f(start+8*i+7);
    }
    long start_rem = start + 8*(len_g);
    for(size_t i = start_rem; i<end; i++) {
      f(i);
    }
#endif

  } else if (true || granularity == 0) {

    size_t len = end-start;
    size_t eightNworkers = 8*num_workers();
    if(granularity == 0) {
      long oriGran = granularity;
      const long maxGran = 2048;
      //const long maxGran = 8;
      //granularity = (len + 8*num_workers() -1 )/(8*num_workers());
      granularity = (len + eightNworkers -1 )/(eightNworkers);
      if(granularity > maxGran)
	granularity = maxGran;
    }

    if(len == 0)
      return;
#if 0
    int size = end - start;
    // For eager
    //parallel_for_eager(start, end, f, granularity, true, 0, 0);
    //return;

    // For lazy
    //parallel_for_eager(start, end, f, granularity, true, 0, size+1);
    //return;

    //size_t nWorkers = num_workers();
    size_t static_range = size/(eightNworkers);
    if(delegate_work == 0) {
      //parallel_for_seq(start, end, f, granularity, true);
      //parallel_for_recurse(start, end, f, granularity, true);
      delegate_work++;
      parallel_for_eager(start, end, f, granularity, true, 0, static_range);
      delegate_work--;
    } else {
      parallel_for_recurse(start, end, f, granularity, true);
    }
#else
    if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
      delegate_work++;
      //printf("[%d] delegate_work: %d\n", threadId, delegate_work);
      void* resumeCtx[64];
      resumeCtx[17] = (void*)num_workers();
      resumeCtx[19] = (void*)threadId;
      resumeCtx[23] = (void*)3;
      //printf("\n[%d]Start paralell static: %zu %zu ctx:%p granularity: %ld delegate_work=%d\n", threadId, start, end, resumeCtx, granularity, delegate_work);
      parallel_for_static(start, end, f, granularity, true, (void**)resumeCtx);
      //parallel_for_recurse(start, end, f, granularity, true);
      //parallel_for_eager(start, end, f, granularity, true, 0, 0);
      //printf("[%d]End paralell static: %zu %zu\n", threadId, start, end);
      delegate_work--;
    } else {
      parallel_for_recurse(start, end, f, granularity, true);
      //parallel_for_eager(start, end, f, granularity, true, 0, 0);
    }
#endif
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }

#endif

}


} // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

