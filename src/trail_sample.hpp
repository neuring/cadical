#ifndef _trail_sample_hpp_INCLUDED
#define _trail_sample_hpp_INCLUDED

#include "cema.hpp"

namespace CaDiCaL {

struct CEMACollector {
  CEMA true_stability;
  CEMA false_stability;
  int64_t last_updated;

  CEMACollector(double alpha) 
  :
    true_stability(CEMA(alpha)),
    false_stability(CEMA(alpha)),
    last_updated(0)
  {}

  CEMACollector() {}
};


}

#endif
