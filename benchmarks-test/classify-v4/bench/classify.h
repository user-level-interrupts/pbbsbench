constexpr int max_value = 255;
using value = unsigned char; 
using row = parlay::sequence<value>;
using rows = parlay::sequence<row>;

struct feature {
  bool discrete; // discrete (true) or continuous (false)
  int num;       // max value of feature
  row vals;      // the sequence of values for the feature
  feature(bool discrete, int num) : discrete(discrete), num(num) {}
  /** ORIGINA: */
  // feature(bool d, int n, row v) : discrete(d), num(n), vals(v) {} /** TODO: rewrite vals(v) to bypass default copy constructor */
  /** DEBUG: bypass default sequence copy constructor */
  feature(bool d, int n, row &v) : discrete(d), num(n) {
    vals.copy_from(v);
  }
  /** PRR: DAC */
  feature(bool d, int n, row &v, bool x) : discrete(d), num(n) {
    vals.copy_from_dac(v);
  }

};

using features = parlay::sequence<feature>;

row classify(features const &Train, rows const &Test, bool verbose);