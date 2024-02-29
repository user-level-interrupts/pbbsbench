// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <algorithm>
#include "common/time_loop.h"
#include "common/geometry.h"
#include "common/geometryIO.h"
#include "common/parseCommandLine.h"
#include "parlay/primitives.h"
#include "delaunay.h"

#include <sys/time.h>
using namespace std;
using namespace benchIO;

#include<set>
#include<map>

std::map<long unsigned , std::set<long unsigned>> taskLen2Gran;
std::map<long unsigned , std::set<long unsigned>> taskLen2Iteration;

#ifdef STATS_OVER_TIME
extern "C"{
  extern void initworkers_env();
  extern void initperworkers_sync(int threadid, int setAllowWS);
  extern void deinitperworkers_sync(int threadId, int clearNotDone);
  extern void deinitworkers_env();

  extern unsigned long long __cilkrts_getticks(void);
}
#endif

// *************************************************************
//  TIMING
// *************************************************************

void timeDelaunay(parlay::sequence<point> &pts, int rounds, char* outFile) {
  triangles<point> R;
#ifdef STATS_OVER_TIME
  struct timeval t1, t2;
  unsigned long long time0 = __cilkrts_getticks();
  initworkers_env();
  initperworkers_sync(0,1);
  time_loop(rounds, 1.0,
	    [&] () {R.P.clear(); R.T.clear();},
	    [&] () {R = delaunay(pts);},
	    [&] () {});
  deinitperworkers_sync(0,1);
  deinitworkers_env();
  unsigned long long time1 = __cilkrts_getticks();
  printf("PBBS-time:%f,cycle:%llu\n", runtime_ms/1000.0, time1-time0);
#else
  time_loop(rounds, 1.0,
	    [&] () {R.P.clear(); R.T.clear();},
	    [&] () {R = delaunay(pts);},
	    [&] () {});
#endif
  cout << endl;
  if (outFile != NULL) writeTrianglesToFile(R, outFile);
}

int main(int argc, char* argv[]) {
  commandLine P(argc,argv,"[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);

  parlay::sequence<point> PI = readPointsFromFile<point>(iFile);
  timeDelaunay(PI, rounds, oFile);
}
