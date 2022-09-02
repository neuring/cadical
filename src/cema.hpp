#ifndef _cema_hpp_INCLUDED
#define _cema_hpp_INCLUDED

#include<cmath>
#include<iostream>

namespace CaDiCaL {

struct Internal;

struct CEMA {

  double cumulative_part;
  double exponential_part;
  double time;
  double cumulative_factor; // always (1 - alpha)^(time + 1)

  CEMA()
  :
    cumulative_part(0), 
    exponential_part(0), 
    time(0), 
    cumulative_factor(1) 
  {}

  double value();

  void  update(double next_value, double alpha);

  void bulk_update(double next_values, int repetition, double alpha);
};

}
#endif
