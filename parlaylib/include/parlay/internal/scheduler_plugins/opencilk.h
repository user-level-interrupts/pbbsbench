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
}

#define getSP(sp) asm volatile("#getsp\n\tmovq %%rsp,%[Var]" : [Var] "=r" (sp))

#define callerTrashed() asm volatile("# all caller saved are trashed here" : : : "rdi", "rsi", "r8", "r9", "r10", "r11", "rdx", "rcx", "rax")
#define calleeTrashed() asm volatile("# all callee saved are trashed here" : : : "rbx", "r12", "r13", "r14", "r15", "rax")
#define intregTrashed() asm volatile("# all integer saved are trashed here" : : : "rbx", "r12", "r13", "r14", "r15", "rax", "rdi", "rsi", "r8", "r9", "r10", "r11", "rdx", "rcx", "xmm0")


 extern std::map<long unsigned , std::set<long unsigned>> taskLen2Gran;
 extern std::map<long unsigned , std::set<long unsigned>> taskLen2Iteration;

 #define barrier() asm volatile ("lfence":::)

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
 __attribute__((noinline)) void rcf(size_t start, size_t end, F f,
			  long granularity,
			  bool conservative) {
   //cilk_for (size_t i=start; i < end; i++) wrapperFi(f, i);
   cilk_for (size_t i=start; i < end; i++) f(i);
  }

 template <typename F>
 __attribute__((noinline)) void run_recursive_cilk_for(size_t start, size_t end, F f,
						long granularity,
						bool conservative) {


   size_t len  = end-start;
   if ((end - start) <= static_cast<size_t>(granularity)) {
     for (size_t i=start; i < end; i++) f(i);
     return;
   }

   size_t div = (len + num_workers()/4 -1 )/(num_workers()/4);
   if (div <= 0)
     div = 1;

   for (int i=0; i<num_workers()/4; i++){
     size_t start_2 = start+div*(i);
     size_t end_2 = start+div*(i+1);
     if (start_2 > end)
       start_2 = end;

     if (end_2 > end)
       end_2 = end;

     if(start_2 == end_2)
       break;

     cilk_spawn run_recursive_cilk_for(start_2, end_2, f, granularity, true);
   }
   cilk_sync;

 }

 template <typename F>
 __attribute__((noinline)) void parallel_for_real_eager(size_t start, size_t end, F f,
				    long granularity,
						     bool conservative, int depth);
 template <typename F>
__attribute__((noinline))
void parallel_for_real_eager_wrapper(size_t start, size_t end, F f,
				    long granularity,
				     bool conservative, int depth,
				     void** readyCtx) {
  int bWorkNotPush;
  push_workctx_eager(readyCtx, &bWorkNotPush);
  parallel_for_real_eager(start, end, f, granularity, true, depth);
  pop_workctx_eager(readyCtx, &bWorkNotPush);
  return;
}

 template <typename F>
 __attribute__((noinline)) void parallel_for_recurse_rcf(size_t start, size_t end, F f,
			  long granularity,
			  bool conservative) {

 tail_recurse:
   if ((end - start) <= static_cast<size_t>(granularity)) {
     rcf(start, end, f, granularity, true);
   } else {
     size_t n = end-start;
     size_t mid = (start + n/2);
     cilk_spawn parallel_for_recurse_rcf(start, mid, f, granularity, true);
     start = mid;
     goto tail_recurse;
   }

   cilk_sync;

 }


 template <typename F>
 __attribute__((noinline)) void parallel_for_recurse(size_t start, size_t end, F f,
			  long granularity,
			  bool conservative) {

 tail_recurse:
   if ((end - start) <= static_cast<size_t>(granularity)) {
     for (size_t i=start; i < end; i++) f(i);
   } else {
     size_t n = end-start;
     size_t mid = (start + n/2);
     cilk_spawn parallel_for_recurse(start, mid, f, granularity, true);
     start = mid;
     goto tail_recurse;
   }

   cilk_sync;

 }

 template <typename F>
 void parallel_for_real(size_t start, size_t end, F f,
			  long granularity,
			  bool conservative);

 template <typename F>
__attribute__((noinline))
__attribute__((no_unwind_path))
void parallel_for_real_eager(size_t start, size_t end, F f,
				    long granularity,
						   bool conservative, int depth) {

   volatile int owner = threadId;
   volatile void* readyCtx[64];

   if ((end - start) <= static_cast<size_t>(granularity)) {
     for (size_t i=start; i < end; i++) f(i);
   } else if(depth < 0) {
      parallel_for_recurse(start, end, f, granularity, true);
      //parallel_for_real(start, end, f, granularity, true);
   } else {

     size_t n = end-start;
     size_t mid = (start + (9*(n+1))/16);

     void** ctx = allocate_parallelctx2((void**)readyCtx);
     // Store sp
     //readyCtx[17] = (void*) ((gCntrAba[threadId].ptr<<32) | 0x1);
     getSP(readyCtx[2]);
     readyCtx[23] = NULL;
     readyCtx[21] = NULL;
     readyCtx[24] = (void*) -1;
     readyCtx[19] = (void*)owner;
     readyCtx[20] = (void*)&readyCtx[0];

     int volatile dummy_a= 0;
     asm volatile("movl $0, %[ARG]\n\t":[ARG] "=r" (dummy_a));
     __builtin_uli_save_context_nosp((void*)readyCtx, &&det_cont);
     if(dummy_a) {
       goto det_cont;
     }
     parallel_for_real_eager_wrapper(start, mid, f, granularity, true, depth+1, (void**) &readyCtx[0]);
 det_cont:
     intregTrashed();
     parallel_for_real_eager(mid, end, f, granularity, true, depth+1);
     void* sp2;// = (void*)__builtin_read_sp();
     getSP(sp2);
     if(!sync_eagerd((void**)readyCtx, (int)owner, (void*)readyCtx[2], sp2)) {
       __builtin_uli_save_context_nosp((void*)readyCtx, &&sync_pre_resume_parent);
       if(dummy_a) {
	 goto sync_pre_resume_parent;
       }
       resume2scheduler_eager((void**)readyCtx, 1);

 sync_pre_resume_parent:
       intregTrashed();
       set_joincntr((void**)readyCtx);
     }

     //gCntrAba[threadId].ptr++;
     deallocate_parallelctx((void**)readyCtx);
   }
   return;
 }


 #if 1
 template <typename F>
 void parallel_for_real(size_t start, size_t end, F f,
			  long granularity,
			  bool conservative) {

   if ((end - start) <= static_cast<size_t>(granularity)) {
     for (size_t i=start; i < end; i++) f(i);

   } else {
     size_t n = end-start;
     size_t mid = (start + (9*(n+1))/16);
     cilk_spawn parallel_for_real(start, mid, f, granularity, true);
     parallel_for_real(mid, end, f, granularity, true);
     cilk_sync;
   }
 }

 template <typename F>
 void parallel_for_4spawn(size_t start, size_t end, F f,
			  long granularity,
			  bool conservative) {

   if(end - start <= 0)
     return;

   if ((end - start) <= static_cast<size_t>(granularity)) {
     for (size_t i=start; i < end; i++) f(i);

   } else {
 #if 0
     size_t n = end-start;
     size_t mid1 = (start + n/4);
     size_t mid2 = (start + n/2);
     size_t mid3 = (start + 3*n/4);
     cilk_spawn parallel_for_4spawn(start, mid1, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid1, mid2, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid2, mid3, f, granularity, true);
     parallel_for_4spawn(mid3, end, f, granularity, true);
     cilk_sync;
 #else

 #if 0
     size_t n = end-start;
     size_t mid1 = (start + n/8);
     size_t mid2 = (start + n/4);
     size_t mid3 = (start + 3*n/8);
     size_t mid4 = (start + n/2);
     size_t mid5 = (start + 5*n/8);
     size_t mid6 = (start + 3*n/4);
     size_t mid7 = (start + 7*n/8);

     cilk_spawn parallel_for_4spawn(start, mid1, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid1, mid2, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid2, mid3, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid3, mid4, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid4, mid5, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid5, mid6, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid6, mid7, f, granularity, true);
     parallel_for_4spawn(mid7, end, f, granularity, true);
     cilk_sync;
 #else

     size_t n = end-start;
     size_t mid1 = (start + n/16);
     size_t mid2 = (start + 2*n/16);
     size_t mid3 = (start + 3*n/16);
     size_t mid4 = (start + 4*n/16);
     size_t mid5 = (start + 5*n/16);
     size_t mid6 = (start + 6*n/16);
     size_t mid7 = (start + 7*n/16);
     size_t mid8 = (start + 8*n/16);
     size_t mid9 = (start + 9*n/16);
     size_t mid10 = (start + 10*n/16);
     size_t mid11 = (start + 11*n/16);
     size_t mid12 = (start + 12*n/16);
     size_t mid13 = (start + 13*n/16);
     size_t mid14 = (start + 14*n/16);
     size_t mid15 = (start + 15*n/16);



     cilk_spawn parallel_for_4spawn(start, mid1, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid1, mid2, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid2, mid3, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid3, mid4, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid4, mid5, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid5, mid6, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid6, mid7, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid7, mid8, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid8, mid9, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid9, mid10, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid10, mid11, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid11, mid12, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid12, mid13, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid13, mid14, f, granularity, true);
     cilk_spawn parallel_for_4spawn(mid14, mid15, f, granularity, true);
     parallel_for_4spawn(mid15, end, f, granularity, true);

    cilk_sync;


#endif

#endif
  }
}


#else

template <typename F>
__attribute__((noinline))
void parallel_for_real(size_t start, size_t end, F f,
		       long granularity,
		       bool conservative) {


  for(size_t j=0; j<granularity; j++) {
    f(start+end*granularity+j);
  }


}

#endif

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


template <typename F>
__attribute__((noinline))
__attribute__((no_unwind_path))
void parallel_for_static(size_t start, size_t end, F f,
		       long granularity,
			 bool conservative, void** resumeCtx) {

  int size = end - start;
  size_t nWorkers = num_workers();
  size_t static_range = size/nWorkers;

  if(size < granularity ) {
    for(int i=start; i<end; i++) {
      f(i);
    }
  } else {
    if(static_range < 1) {
      static_range = 1;
      nWorkers = size;
    }
    resumeCtx[17] = (void*)nWorkers;

    //printf("det cont: %p\n", &&det_cont);
    volatile void* parallelCtx[64];
    int volatile dummy_a= 0;
    __asm__ volatile("movl $0, %[ARG]\n\t":[ARG] "=r" (dummy_a));
    __builtin_uli_save_context_nosp((void*)parallelCtx, &&det_cont);
    if(dummy_a) {
      goto det_cont;
    }
    for(int i=1; i<nWorkers; i++) {
      push_workmsg((void**)parallelCtx, i);
    }

    //printf("[%d]Start range static: %d %d\n", threadId, start, start+static_range);
    //printf("[%d] paralellCtx[I_RIP]:%p\n", threadId, parallelCtx[1]);
    parallel_for_recurse(start, start+static_range, f, granularity, true);
    //for (size_t i=start; i < start+static_range; i++) f(i);
    return;

  det_cont:
    callerTrashed();
    int start_par = start + threadId*static_range;
    int end_par = start + (threadId+1)*(static_range);
    if(threadId == nWorkers-1)
      end_par = end;

    //printf("[%d] execute work: %p\n", threadId, get_stacklet_ctx()[1]);

    //printf("[%d]Start range static: %d %d\n", threadId, start_par, end_par);
    parallel_for_recurse(start_par, end_par, f, granularity, true);
    //for (size_t i=start_par; i < end_par; i++) f(i);
    //printf("[%d] resumeCtx[17] [%p]: %p Newsp: %p\n", threadId, &resumeCtx[17], resumeCtx[17], get_stacklet_ctx()[18]);

    resume2scheduler((void**)resumeCtx, get_stacklet_ctx()[18]);
  }
}


template <typename F>
__attribute__((noinline)) void parallel_for(size_t start, size_t end, F f,
//inline void parallel_for(size_t start, size_t end, F f,
                         long granularity,
                         bool ) {


#if 1
#if 1
#if 0

  if (granularity == 0) {
    cilk_for(size_t i=start; i<end; i++) f(i);
    //rcf(start, end, f, granularity, true);
    //cilk_for(size_t i=start; i<end; i++) wrapperFi(f, i);
  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
    //for (size_t i=start; i < end; i++) wrapperFi(f, i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }

#else
  //granularity = 0;
  if (granularity == 0 || granularity == -1) {
    long oriGran = granularity;
    const long maxGran = 2048;

    size_t len = end-start;
    //size_t eightNworkers = 8*num_workers();
    granularity = (len + 8*num_workers() -1 )/(8*num_workers());
    if(granularity > maxGran)
      granularity = maxGran;

    if(len == 0)
      return;
#if 1
    //parallel_for_seq(start, end, f, granularity, true);
    parallel_for_recurse(start, end, f, granularity, true);
#else
    if(oriGran == -1) {
      //printf( "Start of parllel_for_static\n" );
      int volatile dummy_a= 0;
      asm volatile("movl $0, %[ARG]\n\t":[ARG] "=r" (dummy_a));
      volatile void* resumeCtx[64];
      resumeCtx[17] = (void*)num_workers();
      resumeCtx[19] = 0;
      resumeCtx[23] = (void *)3;
      //printf("\n[%d]Start paralell static: %d %d ctx:%p\n", threadId, start, end, resumeCtx);
      parallel_for_static(start, end, f, granularity, true, (void**)resumeCtx);

      if(resumeCtx[17] > (void*)1) {
	assert(resumeCtx[17] != 0 && "Synchronizer can not be 0 here");
	__builtin_uli_save_context((void*)resumeCtx, &&sync_pre_resume_parent);
	if(dummy_a) {
	  goto sync_pre_resume_parent;
	}
	//printf("[%d] resumeCtx[17] : [%p]:%p RIP: %p\n", threadId, &resumeCtx[17], resumeCtx[17], resumeCtx[1]);
	suspend2scheduler_shared((void**)resumeCtx);
      sync_pre_resume_parent:
	intregTrashed();
      }

      //printf("[%d]End parallel static: %d %d\n", threadId, start, end);
      //printf("End of parllel_for_static\n");

    } else {
      parallel_for_recurse(start, end, f, granularity, true);
    }
#endif

  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
    //for (size_t i=start; i < end; i++) wrapperFi(f, i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }


#endif

#else

#if  0
  if ( (granularity != 1) || granularity == 0) {
    //cilk_for(size_t i=start; i<end; i++) f(i);
    //run_cilk_for(start, end, f, granularity, true);
    //cilk_for(size_t i=start; i<end; i++) wrapperFi(f, i);

    long maxGran = 1;
    char * sMaxGran = getenv("MAX_GRAN");
    if(sMaxGran)
      maxGran = atol(sMaxGran);


    granularity = (end-start)+1;
    granularity = (granularity + 8*num_workers() -1 )/(8*num_workers());
    //gran = (gran + nBlocks -1 )/(nBlocks);
    if (granularity > maxGran)
      granularity = maxGran;
  }

  assert(granularity != 0);

  if ((end - start) <= static_cast<size_t>(granularity)) {
    //for (size_t i=start; i < end; i++) f(i);
    //cilk_for (size_t i=start; i < end; i++) wrapperFi(f, i);
    //if((end - start) <= static_cast<size_t>(16))
      for (size_t i=start; i < end; i++) wrapperFi(f, i);
      //else
      //run_cilk_for(start, end, f, granularity, true);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for_real(start, mid, f, granularity, true);
    parallel_for_real(mid, end, f, granularity, true);
    cilk_sync;
  }
#else

  long long unsigned startTime, endTime;
  size_t nIteration = end-start+1;
  barrier();
  startTime = _rdtsc();
  for (size_t i=start; i < end; i++)  {
    f(i);
  }
  barrier();
  endTime = _rdtsc();

  long long unsigned taskLen =0;
  if (nIteration)
    taskLen = (endTime - startTime)/nIteration;

  long unsigned granType =1000;
  //printf("Granularity: %ld\n", granularity);
  if(enableInstrument) {
    if(granularity == 0) {
      histGran[0]++;
      granType = 0;
    } else if(granularity == 1) {
      histGran[1]++;
      granType = 1;
    } else if(granularity > 1 && granularity < 129) {
      histGran[2]++;
      granType = 2;
    } else if(granularity > 128 && granularity < 2001) {
      histGran[3]++;
      granType = 3;
    }else if(granularity > 2000 && granularity < 5001) {
      histGran[4]++;
      granType = 4;
    }else if(granularity > 5000 && granularity < 9001)  {
      histGran[5]++;
      granType = 5;
    }else if(granularity > 9000 && granularity < 33001) {
      histGran[6]++;
      granType = 6;
    }else if(granularity > 33000)  {
      granType = 7;
      histGran[7]++;
    }

    long unsigned iterationType = 10000;
    if(nIteration < 1000) {
      iterationType=0;
      histIteration[0]++;
    } else if(nIteration < 10000 && nIteration >= 1000) {
      histIteration[1]++;
      iterationType=1;
    } else if(nIteration < 100000 && nIteration >= 10000) {
      histIteration[2]++;
      iterationType=2;
    } else if(nIteration < 1000000 && nIteration >= 100000) {
      histIteration[3]++;
      iterationType=3;
    } else if(nIteration < 10000000 && nIteration >= 1000000) {
      histIteration[4]++;
      iterationType=4;
    } else if(nIteration < 100000000 && nIteration >= 10000000) {
      histIteration[5]++;
      iterationType=5;
    } else if(nIteration < 1000000000 && nIteration >= 100000000) {
      histIteration[6]++;
      iterationType=6;
    } else if(nIteration >= 1000000000) {
      histIteration[7]++;
      iterationType=7;
    }

    if(taskLen < 1000) {
      histMedTaskLen[0]++;
      if(!taskLen2Gran.count(0))
	taskLen2Gran[0] = {};
      taskLen2Gran[0].insert(granType);

      if(!taskLen2Iteration.count(0))
	taskLen2Iteration[0] = {};
      taskLen2Iteration[0].insert(iterationType);

    } else if(taskLen>=1000 && taskLen<5000) {
      if(!taskLen2Gran.count(1))
	taskLen2Gran[1] = {};
      taskLen2Gran[1].insert(granType);

      if(!taskLen2Iteration.count(1))
	taskLen2Iteration[1] = {};
      taskLen2Iteration[1].insert(iterationType);


      histMedTaskLen[1]++;
    } else if(taskLen>=5000 && taskLen<10000) {
      if(!taskLen2Gran.count(2))
	taskLen2Gran[2] = {};
      taskLen2Gran[2].insert(granType);

      if(!taskLen2Iteration.count(2))
	taskLen2Iteration[2] = {};
      taskLen2Iteration[2].insert(iterationType);


      histMedTaskLen[2]++;
    } else if(taskLen>=10000 && taskLen<50000) {
      if(!taskLen2Gran.count(3))
	taskLen2Gran[3] = {};
      taskLen2Gran[3].insert(granType);

      if(!taskLen2Iteration.count(3))
	taskLen2Iteration[3] = {};
      taskLen2Iteration[3].insert(iterationType);


      histMedTaskLen[3]++;
    } else if(taskLen>=50000 && taskLen<100000) {
      if(!taskLen2Gran.count(4))
	taskLen2Gran[4] = {};
      taskLen2Gran[4].insert(granType);

      if(!taskLen2Iteration.count(4))
	taskLen2Iteration[4] = {};
      taskLen2Iteration[4].insert(iterationType);


      histMedTaskLen[4]++;
    } else if(taskLen>=100000 && taskLen<500000) {
      if(!taskLen2Gran.count(5))
	taskLen2Gran[5] = {};
      taskLen2Gran[5].insert(granType);

      if(!taskLen2Iteration.count(5))
	taskLen2Iteration[5] = {};
      taskLen2Iteration[5].insert(iterationType);


      histMedTaskLen[5]++;
    } else if(taskLen>=500000 && taskLen<1000000) {
      if(!taskLen2Gran.count(6))
	taskLen2Gran[6] = {};
      taskLen2Gran[6].insert(granType);

      if(!taskLen2Iteration.count(6))
	taskLen2Iteration[6] = {};
      taskLen2Iteration[6].insert(iterationType);


      histMedTaskLen[6]++;
    } else if(taskLen>=1000000) {
      if(!taskLen2Gran.count(7))
	taskLen2Gran[7] = {};
      taskLen2Gran[7].insert(granType);

     if(!taskLen2Iteration.count(7))
	taskLen2Iteration[7] = {};
      taskLen2Iteration[7].insert(iterationType);


      histMedTaskLen[7]++;
    }


  }

  // TODO: Update tasklen
  // TODO: Update iteration
  // Dictionary[tasklen][iteration][granularity] = count



#endif

#endif

#else

#if 0
  //cilk_for(size_t i=start; i<end; i++)  f(i);
  cilk_for(size_t i=start; i<end; i++)  wrapperFi(f, i);

#else

  long nBlocks = 8;
  char * sNblocks = getenv("NBLOCKS_PER_WORKER");
  if(sNblocks)
    nBlocks = atol(sNblocks);

  long maxGran = 2048;
  char * sMaxGran = getenv("MAX_GRAN");
  if(sMaxGran)
    maxGran = atol(sMaxGran);


  size_t gran = granularity;
  if(gran == 0) {
    gran = (end-start)+1;
    gran = (gran + nBlocks*num_workers() -1 )/(nBlocks*num_workers());
    //gran = (gran + nBlocks -1 )/(nBlocks);
    if (gran > maxGran)
      gran = maxGran;
  }
  //assert(gran != 0);
  //printf("Gran: %lu\n", gran);
  //gran = 1;

tail_recurse:

  if ((end - start) <= static_cast<size_t>(gran)) {
    //for (size_t i=start; i < end; i++) wrapperFi(f,i);
    for (size_t i=start; i < end; i++) f(i);
    //run_cilk_for(start, end, f, gran, true);
  } else {

#if 0
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for_real(start, mid, f, gran, true);
    //cilk_spawn run_cilk_for(start, mid, f, gran, true);
    parallel_for_real(mid, end, f, gran, true);
    cilk_sync;
  }
#else
    size_t n = end-start;
    //size_t mid = (start + (9*(n+1))/16);
    size_t mid = (start + n/2);
    cilk_spawn parallel_for_recurse(start, mid, f, gran, true);
    start = mid;
    goto tail_recurse;
  }
  cilk_sync;
#endif

#endif

#endif

}


}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

