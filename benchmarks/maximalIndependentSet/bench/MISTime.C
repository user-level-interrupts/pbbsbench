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
#include "parlay/parallel.h"
#include "common/time_loop.h"
#include "common/graph.h"
#include "common/IO.h"
#include "common/graphIO.h"
#include "common/parse_command_line.h"
#include "MIS.h"
using namespace std;
using namespace benchIO;

bool enableInstrument = 0;

#ifdef STATS_OVER_TIME
extern "C"{
  extern void initworkers_env();
  extern void initperworkers_sync(int threadid, int setAllowWS);
  extern void deinitperworkers_sync(int threadId, int clearNotDone);
  extern void deinitworkers_env();
}
#endif


void timeMIS(Graph const &G, int rounds, char* outFile) {
  parlay::sequence<char> flags = maximalIndependentSet(G);
#ifdef STATS_OVER_TIME
  initworkers_env();
  initperworkers_sync(0,1);
  time_loop(rounds, 0.0,
	    [&] () {flags.clear();},
	    [&] () {flags = maximalIndependentSet(G);},
	    [&] () {});
  deinitperworkers_sync(0,1);
  deinitworkers_env();
#else
  time_loop(rounds, 1.0,
	    [&] () {flags.clear();},
	    [&] () {flags = maximalIndependentSet(G);},
	    [&] () {});
#endif
  cout << endl;
  
  auto F = parlay::tabulate(G.n, [&] (size_t i) -> int {return flags[i];});
  writeIntSeqToFile(F, outFile);
}

int main(int argc, char* argv[]) {
  commandLine P(argc, argv, "[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);
  Graph G = readGraphFromFile<vertexId,edgeId>(iFile);
  timeMIS(G, rounds, oFile);
}
