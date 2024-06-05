#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

#include <cstddef>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline size_t num_workers() { return __cilkrts_get_nworkers(); }
inline size_t worker_id() { return __cilkrts_get_worker_number(); }

template <typename Lf, typename Rf>
//inline void par_do(Lf left, Rf right, bool) {
__attribute__((noinline)) void par_do(Lf left, Rf right, bool) {
  cilk_spawn right();
  left();
  cilk_sync;
}

#if 0
template <typename F>
void parallel_for_real(size_t start, size_t end, F f,
                         long granularity,
                         bool conservative) {

  if (end <= start)
    return;

  
  if (granularity == 0) {
    //assert(0 && "cilk for not used");
    cilk_for(size_t i=start; i<end; i++) f(i);
  }
#if 0
  else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for_real(start, mid, f, granularity, true);
    parallel_for_real(mid, end, f, granularity, true);
    cilk_sync;
  }
#endif  
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
//inline void parallel_for(size_t start, size_t end, F f,
__attribute__((noinline)) void parallel_for(size_t start, size_t end, F f,
                         long granularity,
                         bool ) {
  
  //if (end <= start)
  //  return;
  granularity=16;
#if 0
  size_t limit = end - start + 1;
  size_t workers = num_workers();
  granularity = (limit + (workers *8 ) - 1) / ( workers *8);
  const long maxgrain=32;
  if (granularity > maxgrain)
    granularity = maxgrain;
  //assert(granularity == 1);
#endif
#if 0
  //granularity=0;
  //if (granularity == 0) {
    //#pragma cilk grainsize 1
    //assert(0 && "cilk for not used");
    //cilk_for(size_t i=start; i<end; i++) f(i);
    //cilk_for(size_t i=start; i<end; i++) f(i);
  
  for(size_t i=start; i<end; i++) {
    printf("i: %zu  limit: %zu startL %zu end: %zu\n", i, limit, start, end);
    f(i);
  }
    //}
#else

  //printf("start: %zu end: %zu\n", start, end);
  //for(size_t i=start; i<end; i+=granularity) {
  cilk_for(size_t i=0; i<(end-start)/granularity; i+=1) {
    for(size_t j=0; j<granularity; j++) {
      size_t index = i*granularity+j+start;
      //printf("i: %zu j: %zu  granularity: %zu limit: %zu limit/granularity: %zu index: %zu\n", i, j, granularity, limit, limit/granularity, index);
      f(index);
    }
    //parallel_for_real(start, i, f, granularity, true);
  }

    
  for(size_t j=start+((end-start)/granularity)*granularity; j<end; j++) {
    //printf("index leftovers: %zu\n", j);
    f(j);
  }
    
#endif
#if 0  
  else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for_real(start, mid, f, granularity, true);
    parallel_for_real(mid, end, f, granularity, true);
    cilk_sync;
  }
#endif
  
}
  

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

