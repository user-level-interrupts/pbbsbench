#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

#include <cstddef>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include <x86intrin.h>

#include<set>
#include<map>

/// DEBUG: my own timer
#include <chrono> 

extern bool enableInstrument;
extern long unsigned histGran[8];
extern long unsigned histMedTaskLen[8];
extern long unsigned histIteration[8];

extern "C"{
// for __builtin_uli_lazyd_inst intrinsic
extern void lazydIntrumentLoop (const char * , size_t , size_t , 
                                const char * , size_t , long , int );
extern void lazydProfilingPerf (int , unsigned long , size_t , int , 
                                const char *, const char *, const char *, const char *);
extern bool instrumentTimeLoopOnly;

extern __thread int threadId;

extern void suspend2scheduler_shared(void** resumeCtx);
extern void resume2scheduler(void** resumeCtx, void* newsp);
extern void push_workmsg(void** parallelCtx, int owner);

extern void **get_stacklet_ctx();
extern unsigned cilkg_nproc;
extern int targetTable[144][2];

extern __thread int initDone;
extern __thread int delegate_work;
extern __thread unsigned long long nTask;

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
#elif defined(DELEGATEPRL)
#pragma message (" DELEGATEPRL_ENABLED ")
#elif defined(DELEGATEPRCPRL)
#pragma message (" DELEGATEPRCPRL_ENABLED ")
#else
#pragma message (" UNKNOWN_TASK_SCHEDULER_ENABLED ")
#endif

#ifdef NOOPT
#pragma message (" NOOPT_ENABLED ")
#endif

#if defined(DELEGATEPRC) || defined(DELEGATEPRL) || defined(DELEGATEPRCPRL) 
#pragma message (" DELEGATEWORK_ENABLED ")
#define DELEGATEWORK_ENABLED
#endif

#ifndef MAXGRAINSIZE
#define MAXGRAINSIZE 2048
#endif

#pragma message (" MAXGRAINSIZE " STRING(MAXGRAINSIZE))

//#define NTASK_ENABLED
#ifdef NTASK_ENABLED   
#pragma message (" NTASK_ENABLED ")
#define nTaskAdd(val) nTask+=val
#else
#define nTaskAdd(val)
#endif

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

  size_t len = end - start;
  size_t len_g = len/granularity;
  nTaskAdd(len_g);
  if (len_g >= 1) {
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
    for (size_t i=start; i < end; i++) {
      f(i);
    }
  }
}

template <typename F>
__attribute__((noinline))
void parallel_for_recurse_seq(size_t start, size_t end, F f, long granularity, bool conservative, long threshold) {

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
    parallel_for_static_wrapper(start, end, f, granularity, static_range, nWorkers, parallelCtx);
    resume2scheduler((void**)resumeCtx, get_stacklet_ctx()[18]);
  }
  return;
}

/* == part of prr project ====================================== */
template <typename F>
inline void parallel_for_ef(size_t start, size_t end, F f, long granularity, bool conservative) 
{   
    // /** DEBUG: parallel_for_ef timer */
    // auto start = std::chrono::high_resolution_clock::now();
    
    // instrumentation
    #ifdef ASSERT
    assert(delegate_work == 0 && "parallel_for_dac should only be called in sequential region!");
    __builtin_uli_lazyd_perf((void *)lazydProfilingPerf, 1, end-start, granularity, delegate_work);
    #endif
    if ((end - start) <= static_cast<size_t>(granularity)) {
        for (size_t i=start; i < end; i++) f(i);
    } else {
        size_t len = end-start;
        /** ORIGINAL: */
        if(granularity == 0) {
            long oriGran = granularity;
            size_t eightNworkers = 8*num_workers();
            const long longGrainSize = MAXGRAINSIZE;
            const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
            granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
        }
        /** DEBUG: uniform granularity */
        // long oriGran = granularity;
        // size_t eightNworkers = 8*num_workers();
        // const long longGrainSize = MAXGRAINSIZE;
        // const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
        // granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
        
        if(len == 0)
            return;

        /** DEBUG: IF version */
    #ifdef IF_VERSION
        if (end-start > num_workers() && end-start > granularity && initDone == 1 && threadId == 0) {
            // std::cout << "parallel_for_static clause !\n";
            delegate_work++;
            parallel_for_static(start, end, f, granularity, conservative);
            delegate_work--;
        } else {
            std::cout << "parallel_for_recurse clause ! range=" << end-start << ", granularity=" << granularity << ", nwkrs=" << num_workers() << ", initdone=" << initDone << ", threadId=" << threadId << std::endl;
            delegate_work++;
            size_t eightNworkers = (num_workers()+2)/2;
            long thres = (len)/(eightNworkers);
            if(thres > granularity) {
                parallel_for_recurse(start, end, f, granularity, true);
            } else {
                parallel_for_recurse(start, end, f, granularity, true);
            }
            delegate_work--;
        }
    #else
        /** DEBUG: NO-IF version */
        // assert(end-start > num_workers() && end-start > granularity && "parallel_for_ef failed num_workers test");
        // assert(initDone == 1 && delegate_work == 0 && threadId == 0 && "parallel_for_ef callsites doesn't satisfy sequential constraint!");
        delegate_work++;
        parallel_for_static(start, end, f, granularity, conservative);
        delegate_work--;
    #endif
    }
}

template <typename F>
inline void parallel_for_dac(size_t start, size_t end, F f, long granularity, bool conservative) 
{
    // instrumentation
    #ifdef ASSERT
    assert(delegate_work > 0 && "parallel_for_dac should only be called in parallel region!");
    __builtin_uli_lazyd_perf((void *)lazydProfilingPerf, 2, end-start, granularity, delegate_work);
    #endif

    if ((end - start) <= static_cast<size_t>(granularity)) {
        for (size_t i=start; i < end; i++) f(i);
    } else {
        size_t len = end-start;
        /** ORIGINAL: */
        if(granularity == 0) {
            long oriGran = granularity;
            size_t eightNworkers = 8*num_workers();
            const long longGrainSize = MAXGRAINSIZE;
            const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
            granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
        }
        /** DEBUG: uniform granularity */
        // long oriGran = granularity;
        // size_t eightNworkers = 8*num_workers();
        // const long longGrainSize = MAXGRAINSIZE;
        // const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
        // granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;

        if(len == 0)
            return;
        
    #ifdef NO_DAC
        /** mask parallel_for_dac as parallel_for_dac to investigate performance benefits */
        if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
            delegate_work++;
            parallel_for_static(start, end, f, granularity, true);
            delegate_work--;
        } else {
            delegate_work++;
            size_t eightNworkers = (num_workers()+2)/2;
            long thres = (len)/(eightNworkers);
            if(thres > granularity) {
                parallel_for_recurse(start, end, f, granularity, true);
            } else {
                parallel_for_recurse(start, end, f, granularity, true);
            }
            delegate_work--;
        }
    #else 
        /** DEBUG: the else route of parallel_for since delegate_work > 0*/
        delegate_work++;
        size_t eightNworkers = (num_workers()+2)/2;
        long thres = (len)/(eightNworkers);
        if(thres > granularity) {
            parallel_for_recurse(start, end, f, granularity, true);
        } else {
            parallel_for_recurse(start, end, f, granularity, true);
        }
        delegate_work--;

        /** ORIGINAL: */
        // delegate_work++;
        // parallel_for_recurse(start, end, f, granularity, true);
        // delegate_work--;
    #endif
    }
}
/* == end of prr project ====== */

template <typename F> // inline // __attribute__((noinline))
inline void parallel_for(size_t start, size_t end, F f, long granularity, bool conservative) {
#ifdef RUNTIME // only ran after confidence in analysis results
    #ifdef ASSERT
    __builtin_uli_lazyd_perf((void *)lazydProfilingPerf, 0, end-start, granularity, delegate_work);
    #endif 

    if ((end - start) <= static_cast<size_t>(granularity)) {
        for (size_t i=start; i < end; i++) f(i);
    } else {
        size_t len = end-start;
        /** ORIGINAL: */
        if(granularity == 0) {
            long oriGran = granularity;
            size_t eightNworkers = 8*num_workers();
            const long longGrainSize = MAXGRAINSIZE;
            const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
            granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
        }
        /** DEBUG: uniform granularity */
        // long oriGran = granularity;
        // size_t eightNworkers = 8*num_workers();
        // const long longGrainSize = MAXGRAINSIZE;
        // const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
        // granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
        
        if(len == 0)
            return;

        if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
            delegate_work++;
            parallel_for_static(start, end, f, granularity, true);
            delegate_work--;
        } else {
            delegate_work++;
            size_t eightNworkers = (num_workers()+2)/2;
            long thres = (len)/(eightNworkers);
            if(thres > granularity) {
                parallel_for_recurse(start, end, f, granularity, true);
            } else {
                parallel_for_recurse(start, end, f, granularity, true);
            }
            delegate_work--;
        }
    }
#else // analysis result + runtime instrumentation
    #if defined(BUILTIN) // Chrisma's buildin instrumentation builtin intrinsic
    __builtin_uli_lazyd_inst((void*)lazydIntrumentLoop, nullptr, end-start, granularity, delegate_work);
    #endif 
    delegate_work++;
    cilk_for(size_t i = start; i < end; i++) f(i);
    delegate_work--;
#endif


// #if defined(OPENCILKDEFAULT)
//   // Default
//   if (granularity == 0) {
//     //parallel_for_noinline(start, end, f);
//     cilk_for(size_t i=start; i<end; i++) f(i);
//   } else if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
// #ifdef NOOPT
//     cilk_for(size_t i=start; i<end; i++) f(i);
// #else
//     size_t n = end-start;
//     size_t mid = (start + (9*(n+1))/16);
//     cilk_spawn parallel_for(start, mid, f, granularity, true);
//     parallel_for(mid, end, f, granularity, true);
//     cilk_sync;
// #endif
//   }

// #elif defined(PUREPRL)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     // PUREPRL
//     if (granularity == 0) {
//       size_t len = end-start;
//       const long longGrainSize = 2048;
//       //const long longGrainSize = 8;
//       size_t eightNworkers = 8*num_workers();
//       long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     } 
//     parallel_for_seq(start, end, f, granularity, true);
//   }

// #elif defined(PARLAYREC_NOOPT)

//   // Default
//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//     return;
//   }

//   size_t len = end-start;
//   const long longGrainSize = 2048;
//   size_t eightNworkers = 8*num_workers();
//   long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//   granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;

//   parallel_for_recurse(start, end, f, granularity, true);

// #elif defined(OPENCILKDEFAULT_FINE)

//   // Default
//   if (granularity == 0) {
//     size_t len = end-start;
//     //const long longGrainSize = 2048;
//     const long longGrainSize = 8;
//     size_t eightNworkers = 8*num_workers();
//     long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//     granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;

//     switch(granularity) {
//     case 1:
// #pragma cilk grainsize 1
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 2:
// #pragma cilk grainsize 2
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 3:
// #pragma cilk grainsize 3
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 4:
// #pragma cilk grainsize 4
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 5:
// #pragma cilk grainsize 5
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 6:
// #pragma cilk grainsize 6
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 7:
// #pragma cilk grainsize 7
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     case 8:
//     default:
// #pragma cilk grainsize 8
//       cilk_for(size_t i=start; i<end; i++) f(i);
//       return;
//     }

//   } else if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     size_t n = end-start;
//     size_t mid = (start + (9*(n+1))/16);
//     cilk_spawn parallel_for(start, mid, f, granularity, true);
//     parallel_for(mid, end, f, granularity, true);
//     cilk_sync;
//   }


// #elif defined(PARLAYREC)

//   if (granularity == 0) {
//     size_t len = end-start;
//     const long longGrainSize = 2048;
//     //const long longGrainSize = 8;
//     size_t eightNworkers = 8*num_workers();
//     long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//     granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;

//     parallel_for_recurse(start, end, f, granularity, true);
//   } else if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     size_t n = end-start;
//     size_t mid = (start + (9*(n+1))/16);
//     cilk_spawn parallel_for(start, mid, f, granularity, true);
//     parallel_for(mid, end, f, granularity, true);
//     cilk_sync;
//   }

// #elif defined(DELEGATEEAGERPRC)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     size_t len = end-start;
//     if(granularity == 0) {
//       long oriGran = granularity;
//       size_t eightNworkers = 8*num_workers();
//       const long longGrainSize = 2048;
//       //const long longGrainSize = 8;
//       const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     }

//     if(len == 0)
//       return;

//     if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
//       delegate_work++;
//       parallel_for_static(start, end, f, granularity, true);
//       delegate_work--;
//     } else {
//       //parallel_for_recurse(start, end, f, granularity, true);
//       parallel_for_eager(start, end, f, granularity, true, 0, 0, 0);
//     }
//   }

// #elif defined(PRCPRL)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {

//     long thresholdprl = 0;
//     size_t len = end-start;
//     if(granularity == 0) {
//       size_t len = end-start;
//       size_t eightNworkers = 8*num_workers();
//       //const long longGrainSize = 2048;
//       const long longGrainSize = 8;
//       const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       thresholdprl = smallGrainSize;
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     }
//     if(len == 0)
//       return;

//     parallel_for_recurse_seq(start, end, f, granularity, true, thresholdprl);
//   }

// #elif defined(DELEGATEEAGERPRL)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     size_t len = end-start;
//     //if(true || granularity == 0) {
//     if(granularity == 0) {
//       long oriGran = granularity;
//       size_t eightNworkers = 8*num_workers();
//       //const long longGrainSize = 2048;
//       const long longGrainSize = 8;
//       const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     }

//     if(len == 0)
//       return;

//     if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
//       delegate_work++;
//       parallel_for_static(start, end, f, granularity, true);
//       delegate_work--;
//     } else {
//       parallel_for_seq(start, end, f, granularity, true);
//     }
//   }

// #elif defined(DELEGATEPRC)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     size_t len = end-start;
//     if(granularity == 0) {
//       long oriGran = granularity;
//       size_t eightNworkers = 8*num_workers();
//       const long longGrainSize = 2048;
//       //const long longGrainSize = 8;
//       const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     }

//     if(len == 0)
//       return;

//     if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
//       delegate_work++;
//       parallel_for_static(start, end, f, granularity, true);
//       delegate_work--;
//     } else {
//       delegate_work++;
//       parallel_for_recurse(start, end, f, granularity, true);
//       delegate_work--;
//     }
//   }

// #elif defined(DELEGATEPRL)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {
//     size_t len = end-start;
//     if(granularity == 0) {
//       long oriGran = granularity;
//       size_t eightNworkers = 8*num_workers();
//       //const long longGrainSize = 2048;
//       const long longGrainSize = 8;
//       const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     }

//     if(len == 0)
//       return;

//     if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
//       delegate_work++;
//       parallel_for_static(start, end, f, granularity, true);
//       delegate_work--;
//     } else {
//       //size_t eightNworkers = 8*num_workers();
//       size_t eightNworkers = (num_workers()+2)/2;
//       long thres = (len)/(eightNworkers);
//       if(thres > granularity) {
// 	parallel_for_recurse_seq2(start, end, f, granularity, true, thres);
//       } else {
// 	parallel_for_recurse(start, end, f, granularity, true);
//       }

//     }
//   }

// #elif defined(DELEGATEPRL_NOOPT)

//   if (granularity >= 0 && (end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//     return;
//   }

//   size_t len = end-start;
//   long oriGran = granularity;
//   size_t eightNworkers = 8*num_workers();
//   const long longGrainSize = 8;
//   const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//   granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;

//   if(len == 0)
//     return;

//   if(end-start > num_workers() && end-start > granularity && delegate_work == 0 && initDone == 1 && threadId == 0) {
//     delegate_work++;
//     parallel_for_static(start, end, f, granularity, true);
//     delegate_work--;
//   } else {

// #if 0
//     if(oriGran == -1) {
//       parallel_for_recurse_seq2(start, end, f, granularity, true, end-start);
//       return;
//     }
// #endif

//     //size_t eightNworkers = 8*num_workers();
//     size_t eightNworkers = (num_workers()+2)/2;
//     long thres = (len)/(eightNworkers);

// #if 1
//     delegate_work++;
//     if(thres > granularity) {
//       parallel_for_recurse_seq2(start, end, f, granularity, true, thres);
//     } else {
//       parallel_for_recurse(start, end, f, granularity, true);
//     }
//     delegate_work--;
// #else
//     delegate_work++;
//     parallel_for_recurse_seq2(start, end, f, granularity, true, end-start);
//     delegate_work--;
// #endif
//   }

// #elif defined(EAGERPRC)

//   if ((end - start) <= static_cast<size_t>(granularity)) {
//     for (size_t i=start; i < end; i++) f(i);
//   } else {

//     size_t len = end-start;
//     if(granularity == 0) {
//       size_t eightNworkers = 8*num_workers();
//       const long longGrainSize = 2048;
//       //const long longGrainSize = 8;
//       const long smallGrainSize = (len + eightNworkers -1 )/(eightNworkers);
//       //long thresholdprl = smallGrainSize;
//       granularity = smallGrainSize > longGrainSize ? longGrainSize : smallGrainSize;
//     }
//     if(len == 0)
//       return;
//     delegate_work++;
//     parallel_for_eager(start, end, f, granularity, true, 0, len/(num_workers()), len/(8*num_workers()));
//     delegate_work++;
//   }

// #endif
}

/**
* == end of LazydStrategy experiment ======================================
*/


} // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

