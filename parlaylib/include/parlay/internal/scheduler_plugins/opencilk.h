#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

#include <cstddef>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include <x86intrin.h>

#include<set>
#include<map>
#include<unordered_map>
#include<vector>
#include<iostream>

#include <string.h>

extern "C"{
extern __thread int threadId;

extern __thread char parallelFcn[100];

extern void suspend2scheduler_shared(void** resumeCtx);
extern void resume2scheduler(void** resumeCtx, void* newsp);
extern void push_workmsg(void** parallelCtx, int owner);
extern void pollepoch();

extern void **get_stacklet_ctx();
extern unsigned cilkg_nproc;
extern int targetTable[144][2];

extern __thread int initDone;
extern __thread int delegate_work;
extern __thread unsigned long long nTask;

extern void pollpfor();

static void updateState(const char* pforName, size_t tripCount, long grainsize, int depth){
  if(!initDone)
    return;

  char newName[1000];

  // Put the trip count into a bin
  size_t bin = 0;
  if(tripCount <= 10)
    bin = 10;
  else if (tripCount <= 100)
    bin = 100;
  else if (tripCount <= 1000)
    bin = 1000;
  else if (tripCount <= 10000)
    bin = 10000;
  else if (tripCount <= 100000)
    bin = 100000;
  else if (tripCount <= 1000000)
    bin = 1000000;
  else if (tripCount <= 10000000)
    bin = 10000000;
  else if (tripCount <= 100000000)
    bin = 100000000;
  else if (tripCount <= 1000000000)
    bin = 1000000000;
  else
    bin = 10000000000;


  sprintf(newName, "%s+%lu+%ld+%d", pforName, bin, grainsize, depth);
  strcpy(parallelFcn, newName);
  pollpfor();
}

//static void revertToIdle(const char* pforName, size_t tripCount, long grainsize, int depth) {
static void revertToIdle() {
  if(!initDone)
    return;

  strcpy(parallelFcn, "nonpfor");
  pollpfor();
}

}

#define getSP(sp) asm volatile("#getsp\n\tmovq %%rsp,%[Var]" : [Var] "=r" (sp))

#ifndef TASKSCEHEDULER
#define OPENCILKDEFAULT
#else
#pragma message (" TASKSCHEDULER_ENABLED ")
#endif

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#if defined(OPENCILKDEFAULT)
#pragma message (" OPENCILKDEFAULT_ENABLED ")
#elif defined(OPENCILKDEFAULT_FINE)
#pragma message (" OPENCILKDEFAULT_FINE_ENABLED ")
#elif defined(PRC)
#pragma message (" PRC_ENABLED ")
#elif defined(PRL)
#pragma message (" PRL_ENABLED ")
#elif defined(PRCPRL)
#pragma message (" PRCPRL_ENABLED ")
#elif defined(DELEGATEPRC)
#pragma message (" DELEGATEPRC_ENABLED ")
#elif defined(DELEGATEPRCPRL)
#pragma message (" DELEGATEPRCPRL_ENABLED ")
#else
#pragma message (" UNKNOWN_TASK_SCHEDULER_ENABLED ")
#endif

#ifdef NOOPT
#pragma message (" NOOPT_ENABLED ")
#endif

#if defined(DELEGATEPRC) || defined(DELEGATEPRCPRL)
#pragma message (" DELEGATEWORK_ENABLED ")
#define DELEGATEWORK_ENABLED
#endif

#ifndef MAXGRAINSIZE
#define MAXGRAINSIZE 2048
//#define MAXGRAINSIZE 64
//#define MAXGRAINSIZE 256
#endif

#pragma message (" MAXGRAINSIZE " STRING(MAXGRAINSIZE))

//#define NTASK_ENABLED
#ifdef NTASK_ENABLED
#pragma message (" NTASK_ENABLED ")
#define nTaskAdd(val) nTask+=val
#else
#define nTaskAdd(val)
#endif

//extern std::unordered_map<std::string, std::vector<std::tuple<size_t, long, int>>> pfor_metadata;
//static void lazydIntrumentLoop (std::string filename_and_line, size_t tripcount, long grainsize, int depth){
//  pfor_metadata[filename_and_line].push_back(std::make_tuple(tripcount, grainsize, depth));
//}


namespace parlay {

 // IWYU pragma: private, include "../../parallel.h"

 //inline size_t num_workers() { return __cilkrts_get_nworkers(); }
 // The if statement is needed for classify since num_workers is accessed before intitialized?
 inline size_t num_workers() { return (cilkg_nproc == 0)? __cilkrts_get_nworkers() : cilkg_nproc; }
 inline size_t worker_id() { return __cilkrts_get_worker_number(); }
 //inline size_t worker_id() { return threadId; }

 template <typename F>
 __attribute__((noinline)) void wrapperF(F f) {
   f();
 }


static int dummyfcn() {return 0;}

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool) {
   // Cause error in neighbors if not turned on
#ifdef DELEGATEWORK_ENABLED
   nTaskAdd(1);
   delegate_work++;
   cilk_spawn wrapperF(right);
   wrapperF(left);
   cilk_sync;
   delegate_work--;
#else
   cilk_spawn right();
   left();
   cilk_sync;
#endif
 }


template <typename F>
__attribute__((noinline))
void parallel_for_recurse(size_t start, size_t end, F f, long granularity, bool conservative) {
  __builtin_uli_lazyd_inst((void*)updateState, (void*)(updateState), end-start, granularity, delegate_work);
 tail_recurse:
   if ((end - start) <= static_cast<size_t>(granularity)) {
     for (size_t i=start; i < end; i++) f(i);
   } else {
     nTaskAdd(1);
     size_t n = end-start;
     size_t mid = (start + n/2);
     cilk_spawn parallel_for_recurse(start, mid, f, granularity, true);
     start = mid;
     goto tail_recurse;
   }
   cilk_sync;
 }

template <typename F>
__attribute__((noinline))
void parallel_for_recurse_seq(size_t start, size_t end, F f, long granularity, bool conservative, long threshold);

template <typename F>
__attribute__((noinline))
void parallel_for_seq(size_t start, size_t end, F f, long granularity, bool conservative) ;


template <typename F>
__attribute__((noinline))
void parallel_for_seq(size_t start, size_t end, F f,
		       long granularity,
		       bool conservative) {
  __builtin_uli_lazyd_inst((void*)updateState, (void*)(updateState), end-start, granularity, delegate_work);
  size_t len = end - start;
  size_t len_g = len/granularity;
  nTaskAdd(len_g);
  if (len_g >= 1) {
    cilk_for (size_t i=0; i < len_g; i++) {
      //for (size_t i=0; i < len_g; i++) {
      for (size_t j=0; j<granularity; j++) {
	f(start+granularity*i+j);
      }
    }
    long start_rem = start + granularity*(len_g);
    for(size_t i = start_rem; i<end; i++) {
      f(i);
    }
  } else {
    for (size_t i=start; i < end; i++) {
      f(i);
    }
  }
}

template <typename F>
__attribute__((noinline))
void parallel_for_recurse_seq(size_t start, size_t end, F f, long granularity, bool conservative, long threshold) {
  __builtin_uli_lazyd_inst((void*)updateState, (void*)(updateState), end-start, granularity, delegate_work);
 tail_recurse:
  if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else if ((end - start) <= static_cast<size_t>(threshold)) {
    parallel_for_seq(start, end, f, granularity, true);
  } else {
    nTaskAdd(1);
    size_t n = end-start;
    size_t mid = (start + n/2);
    cilk_spawn parallel_for_recurse_seq(start, mid, f, granularity, true, threshold);
    start = mid;
    goto tail_recurse;
  }
  cilk_sync;
}

template <typename F>
__attribute__((noinline))
void parallel_for_recurse_seq2(size_t start, size_t end, F f, long granularity, bool conservative, long threshold) {
  // Error?
  __builtin_uli_lazyd_inst((void*)updateState, (void*)(updateState), end-start, granularity, delegate_work);
 tail_recurse:
  if ((end - start) <= static_cast<size_t>(threshold)) {
    parallel_for_seq(start, end, f, granularity, true);
  } else {
    size_t n = end-start;
    size_t mid = (start + n/2);
    cilk_spawn parallel_for_recurse_seq2(start, mid, f, granularity, true, threshold);
    start = mid;
    goto tail_recurse;
  }
  cilk_sync;
}


#if !(defined(OPENCILKDEFAULT) || defined(OPENCILKDEFAULT_FINE))

template <typename F>
__attribute__((noinline))
void parallel_for_static_wrapper(size_t start, size_t end, F f, long granularity, size_t static_range, size_t nWorkers, void* parallelCtx) {
  size_t start_par_l = start + threadId*static_range;
  size_t end_par_l = start + (threadId+1)*(static_range);
  if(threadId == nWorkers-1) {
    end_par_l = end;
  }

  if(targetTable[threadId][0] != 0) {
    push_workmsg((void**)parallelCtx, targetTable[threadId][0]);
  }
  if(targetTable[threadId][1] != 0) {
    push_workmsg((void**)parallelCtx, targetTable[threadId][1]);
  }

#if defined(DELEGATEPRC)
  parallel_for_recurse(start_par_l, end_par_l, f, granularity, true);
#else
  long thres1 = (end_par_l-start_par_l) / (1*num_workers());
  if(thres1 > granularity) {
    parallel_for_recurse_seq2(start_par_l, end_par_l, f, granularity, true, thres1);
  } else {
    parallel_for_recurse(start_par_l, end_par_l, f, granularity, true);
  }
#endif
}

template <typename F>
__attribute__((noinline))
__attribute__((no_unwind_path))
__attribute__((forkable))
void parallel_for_static(size_t start, size_t end, F f, long granularity, bool conservative) {
  size_t size = end - start;
  size_t nWorkers = num_workers();
  size_t static_range = size/nWorkers;

  void* resumeCtx[64];
  resumeCtx[17] = (void*)num_workers();
  resumeCtx[19] = (void*)threadId;
  resumeCtx[23] = (void*)3;

  void* parallelCtx[64];
  getSP(resumeCtx[2]);
  __builtin_multiret_call(2, 1, (void*)&dummyfcn, (void*)parallelCtx, &&det_cont_static, &&det_cont_static);

  if(targetTable[threadId][0] != 0) {
    push_workmsg((void**)parallelCtx, targetTable[threadId][0]);
  }
  if(targetTable[threadId][1] != 0) {
    push_workmsg((void**)parallelCtx, targetTable[threadId][1]);
  }

#if defined(DELEGATEPRC)
  parallel_for_recurse(start, start+static_range, f, granularity, true);
#else
  long thres0 = (static_range) / (1*num_workers());
  if(thres0 > granularity) {
    parallel_for_recurse_seq2(start, start+static_range, f, granularity, true, thres0);
  } else {
    parallel_for_recurse(start, start+static_range, f, granularity, true);
  }
#endif

  if(resumeCtx[17] > (void*)1) {
    __builtin_multiret_call(2, 1, (void*)&dummyfcn, (void*)resumeCtx, &&sync_pre_resume_parent_static, &&sync_pre_resume_parent_static);
    suspend2scheduler_shared((void**)resumeCtx);
  sync_pre_resume_parent_static: {
    }
  }
  return;

  det_cont_static: {
    __builtin_uli_lazyd_inst((void*)updateState, nullptr, end-start, granularity, delegate_work);
    parallel_for_static_wrapper(start, end, f, granularity, static_range, nWorkers, parallelCtx);
    resume2scheduler((void**)resumeCtx, get_stacklet_ctx()[18]);
  }
  return;
 }

#endif

template <typename F>
inline
void parallel_for(size_t start, size_t end, F f,
                         long granularity,
                         bool ) {

    //__builtin_uli_lazyd_inst((void*)updateState, nullptr, end-start, granularity, delegate_work);
  if(initDone)
    __builtin_uli_lazyd_inst((void*)updateState, (void*)updateState, end-start, granularity, delegate_work);
#if defined(OPENCILKDEFAULT)
  // Default
  if (granularity == 0) {
    //delegate_work++;
    cilk_for(size_t i=start; i<end; i++) f(i);
    //delegate_work--;
  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
#ifdef NOOPT
    #pragma message (" OPENCILK NOOPT_ENABLED ")
    cilk_for(size_t i=start; i<end; i++) f(i);
#else
    //delegate_work++;
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
    //delegate_work--;
#endif
  }

#elif defined(OPENCILKDEFAULT_FINE)

  // Default
  if (granularity == 0) {
    size_t len = end-start;
    const long longGrainSize = MAXGRAINSIZE;
    size_t eightNworkers = 8*num_workers();
    long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
    granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;

    switch(granularity) {
    case 1:
#pragma cilk grainsize 1
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 2:
#pragma cilk grainsize 2
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 3:
#pragma cilk grainsize 3
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 4:
#pragma cilk grainsize 4
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 5:
#pragma cilk grainsize 5
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 6:
#pragma cilk grainsize 6
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 7:
#pragma cilk grainsize 7
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    case 8:
    default:
#pragma cilk grainsize 8
      cilk_for(size_t i=start; i<end; i++) f(i);
      return;
    }

  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }

#elif defined(PRC)

  if (granularity == 0) {
    size_t len = end-start;
    const long longGrainSize = MAXGRAINSIZE;
    size_t eightNworkers = 8*num_workers();
    long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
    granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
    parallel_for_recurse(start, end, f, granularity, true);
  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }

#elif defined(PRL)

  if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t len = end-start;
    if(granularity == 0) {
      size_t eightNworkers = 8*num_workers();
      const long longGrainSize = MAXGRAINSIZE;
      const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
      granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
    }
    if(len == 0)
      return;

    parallel_for_seq(start, end, f, granularity, true);
 }

#elif defined(PRCPRL)

  if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    long thresholdprl = 0;
    size_t len = end-start;
#ifdef NOOPT
    #pragma message (" PRCPRL NOOPT_ENABLED ")
    granularity=0;
#endif
    if(granularity == 0) {
      size_t len = end-start;
      size_t eightNworkers = 8*num_workers();
      const long longGrainSize = MAXGRAINSIZE;
      const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
      thresholdprl = smallGrainSize;
      granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
    }
    if(len == 0)
      return;

    parallel_for_recurse_seq(start, end, f, granularity, true, thresholdprl);
  }

#elif defined(DELEGATEPRC)

  if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t len = end-start;
#ifdef NOOPT
    #pragma message (" DELEGATEPRC NOOPT_ENABLED ")
    granularity=0;
#endif
    if(granularity == 0) {
      long oriGran = granularity;
      size_t eightNworkers = 8*num_workers();
      const long longGrainSize = MAXGRAINSIZE;
      const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
      granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
    }

    if(len == 0)
      return;

    if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
      delegate_work++;
      parallel_for_static(start, end, f, granularity, true);
      delegate_work--;
    } else {
      delegate_work++;
      parallel_for_recurse(start, end, f, granularity, true);
      delegate_work--;
    }
  }

#elif defined(DELEGATEPRCPRL)

  if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {

    size_t len = end-start;
#ifdef NOOPT
    #pragma message (" DELEGATEPRCPRL NOOPT_ENABLED ")
    granularity=0;
#endif
    if(granularity == 0) {
      long oriGran = granularity;
      size_t eightNworkers = 8*num_workers();
      const long longGrainSize = MAXGRAINSIZE;
      const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
      granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
    }

    //if(len == 0)
    //  return;

    //if(delegate_work > 8) {
    //if(end-start+1 < 3) {
    //for (size_t i=start; i < end; i++) f(i);
    //return;
    //}
    //}


    if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
      delegate_work++;
      parallel_for_static(start, end, f, granularity, true);
      delegate_work--;
    } else {
      delegate_work++;
#if 1
      size_t eightNworkers = (num_workers()+2)/2;
      long thres = (len)/(eightNworkers);
      if(thres > granularity) {
	parallel_for_recurse_seq2(start, end, f, granularity, true, thres);
      } else {
	parallel_for_recurse(start, end, f, granularity, true);
      }
#else
      size_t eightNworkers = 8*num_workers();
      const long longGrainSize = MAXGRAINSIZE;
      const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
      long thresholdprl = 0;
      parallel_for_recurse_seq(start, end, f, granularity, true, thresholdprl);
#endif
      delegate_work--;
    }
  }


  if(delegate_work == 0) {
    //__builtin_uli_lazyd_inst((void*)revertToIdle, nullptr, end-start, granularity, delegate_work);
    //strcpy(parallelFcn, "serial");
    revertToIdle();
  }

#endif
}


} // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

