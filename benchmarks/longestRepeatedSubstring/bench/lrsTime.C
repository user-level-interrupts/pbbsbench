// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
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
#include "parlay/primitives.h"
#include "parlay/io.h"
#include "common/IO.h"
#include "common/sequenceIO.h"
#include "common/parse_command_line.h"
#include "common/time_loop.h"

#include "lrs.h"
using namespace std;
using namespace benchIO;
using pstring = parlay::sequence<char>;
using parlay::to_chars;

#ifdef STATS_OVER_TIME
extern "C"{
  extern void initworkers_env();
  extern void initperworkers_sync(int threadid, int setAllowWS);
  extern void deinitperworkers_sync(int threadId, int clearNotDone);
  extern void deinitworkers_env();
}
#endif


void timeLongestRepeatedSubstring(pstring const &s, int rounds, bool verbose, char* outFile) {
  size_t n = s.size();
  auto ss = parlay::map(s, [] (char c) {return (unsigned char) c;});
  result_type R;
#ifdef STATS_OVER_TIME
  initworkers_env();
  initperworkers_sync(0,1);
  time_loop(rounds, 2.0,
	    [&] () {},
	    [&] () {R = lrs(ss);},
	    [&] () {}
	    );
  deinitperworkers_sync(0,1);
  deinitworkers_env();
#else
  time_loop(rounds, 2.0,
	    [&] () {},
	    [&] () {R = lrs(ss);},
	    [&] () {}
	    );
#endif
  cout << endl;
  if (outFile != NULL) {
    auto [len, loc1, loc2] = R;
    pstring nl = parlay::to_sequence("\n");
    parlay::sequence<pstring> x{to_chars(len), nl, to_chars(loc1), nl, to_chars(loc2), nl};
    parlay::chars_to_file(flatten(x), outFile);
  }
}

int main(int argc, char* argv[]) {
  commandLine P(argc,argv,"[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  bool verbose = P.getOption("-v");
  int rounds = P.getOptionIntValue("-r",1);
  //parlay::sequence<char> S = parlay::chars_from_file(iFile, true);
  parlay::sequence<char> S = parlay::to_sequence(parlay::file_map(iFile));
  
  timeLongestRepeatedSubstring(S, rounds, verbose, oFile);
}
