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
}

#if 0
template <typename F>
void parallel_for_real(size_t start, size_t end, F f,
                         long granularity,
                         bool conservative) {

  if (end <= start)
    return;


  if (granularity == 0) {
    assert(0 && "cilk for not used");
    cilk_for(size_t i=start; i<end; i++) f(i);
  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) f(i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for_real(start, mid, f, granularity, true);
    parallel_for_real(mid, end, f, granularity, true);
    cilk_sync;
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
//__attribute__((noinline)) void parallel_for(size_t start, size_t end, F f,
inline void parallel_for(size_t start, size_t end, F f,
                         long granularity,
                         bool ) {


#if 0
  //if (granularity == 0)
  //granularity=1;

  if (granularity == 0) {
    //cilk_for(size_t i=start; i<end; i++) f(i);
    cilk_for(size_t i=start; i<end; i++) wrapperFi(f, i);
  } else if ((end - start) <= static_cast<size_t>(granularity)) {
    //for (size_t i=start; i < end; i++) f(i);
    for (size_t i=start; i < end; i++) wrapperFi(f, i);
  } else {
    size_t n = end-start;
    size_t mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity, true);
    parallel_for(mid, end, f, granularity, true);
    cilk_sync;
  }

#else

  //cilk_for(size_t i=start; i<end; i++)  f(i);
  cilk_for(size_t i=start; i<end; i++)  wrapperFi(f, i);

#endif

}


}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OPENCILK_H_

