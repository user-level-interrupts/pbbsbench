# pbbsbench

The Problem Based Benchmark Suite (PBBS) is a collection
of over 20 benchmarks defined in terms of their IO characteristics.
They are designed to make it possible to compare different algorithms,
or implementations in different programming languages.

Information on the organization and on how to run PBBS can be found on
[this page](https://cmuparlay.github.io/pbbsbench).

# `parallel_for` substitution experiment
This experiment forks the important parts of `pbbs_v2` benchmark suite into three different versions: 
- orig: An exact copy of master branch. code setup in `./benchmarks`, `./common`, and `./parlay`.
- control: code setup in `./benchmarks-orig`, `./common-orig`, and `./parlay-orig`.
- test: code setup in `./benchmarks-test`, `./common-test`, and `./parlay-test`

Both control and test group are used to track code changes made in benchmarks (e.g. `classify`, `delaunayTriangulation`)
as well as parlay libraries that are modified from the original master branch implementation. 

The control group sometimes rewrites the benchmark code slightly to derive better static parallel-region analysis result 
(e.g. `build_tree` in `classify.C` uses recursion that makes its parallel region state overconservative);

The test group is builtin on the control group by substitute parlay functions that eventually calls different versions of
`parallel_for` depending on static parallel-region analysis results. There are three versions: 
- `parallel_for` (the `both` case)
- `parallel_for_ef` (the `defef` case)
- `parallel_for_dac` (the `defdac` case)
Their definitions are provided in `./parlay-test/internal/scheduler_plugins/opencilk.h`

The reference group is an identical copy from the master branch without any code changes (not even those in the control
group). This group is aimed to provide reference parlay library performance in comparison to the other two groups. 

