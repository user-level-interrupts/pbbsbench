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
#include "common/geometry.h"
#include "common/geometryIO.h"
#include "common/parse_command_line.h"
#include "parlay/primitives.h"
#include "common/time_loop.h"
#include "nbody.h"
using namespace std;
using namespace benchIO;

// *************************************************************
//  TIMING
// *************************************************************

#ifdef STATS_OVER_TIME
extern "C"{
  extern void initworkers_env();
  extern void initperworkers_sync(int threadid, int setAllowWS);
  extern void deinitperworkers_sync(int threadId, int clearNotDone);
  extern void deinitworkers_env();
}
#endif


void timeNBody(parlay::sequence<point> const &pts, int rounds, char* outFile) {
  parlay::internal::timer t;
  auto pp = parlay::map(pts, [] (point p) -> particle {return particle(p, 1.0);});
  parlay::sequence<particle*> p = parlay::tabulate(pts.size(), [&] (size_t i) -> particle* {
      return &pp[i];});

  #ifdef BUILTIN
  instrumentTimeLoopOnly = true;
  #endif

#ifdef STATS_OVER_TIME
  initworkers_env();
  initperworkers_sync(0,1);
  time_loop(rounds, 0.0,
	    [&] () {},
	    [&] () {nbody(p);},
	    [&] () {});
  deinitperworkers_sync(0,1);
  deinitworkers_env();
#else
  time_loop(rounds, 1.0,
	    [&] () {},
	    [&] () {nbody(p);},
	    [&] () {});
#endif
  #ifdef BUILTIN
  instrumentTimeLoopOnly = false;
  #endif

  cout << endl;

  auto O = parlay::map(p, [] (particle* p) {
      return point(0.,0.,0.) + p->force;});

  if (outFile != NULL) 
    writePointsToFile(O, outFile);
}

int main(int argc, char* argv[]) {
  commandLine P(argc,argv,"[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);

  parlay::sequence<point> PIn = readPointsFromFile<point>(iFile);
  timeNBody(PIn, rounds, oFile);
}
