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
  double alpha;
  double cumulative_factor; // always (1 - alpha)^(time + 1)

  CEMA(double alpha) 
  : 
    cumulative_part(0), 
    exponential_part(0), 
    time(0), 
    alpha(alpha), 
    cumulative_factor(1 - alpha) 
  {

  }

  CEMA()
  :
    cumulative_part(0), 
    exponential_part(0), 
    time(0), 
    alpha(0), 
    cumulative_factor(1) 
  {}

  double value() {
    return this->exponential_part + this->cumulative_part;
  }

  void  update(double next_value) {
    double new_exponential_part = this->alpha * next_value + (1 - this->alpha) * this->exponential_part;

    if (this->cumulative_part) {
      double new_cumulative_part = (1 - this->alpha) * (this->cumulative_part + (this->cumulative_factor * next_value - this->cumulative_part) / (this->time + 2));
      this->cumulative_part = new_cumulative_part;
    }

    this->exponential_part = new_exponential_part;

    this->time += 1;
    this->cumulative_factor *= (1 - this->alpha);
  }

  void bulk_update(double next_values, int repetition) {
    double exp_repetition = std::pow(1 - this->alpha, repetition);

    double new_exponential_part = (1 - exp_repetition) * next_values + exp_repetition * this->exponential_part;

    if (this->cumulative_part) {
      double new_cumulative_part = exp_repetition * (this->cumulative_part + repetition * (this->cumulative_factor * next_values - this->cumulative_part) / (this->time + repetition + 1));
      this->cumulative_part = new_cumulative_part;
    }

    this->exponential_part = new_exponential_part;

    this->time += repetition;
    this->cumulative_factor *= exp_repetition;
  }
};

}
#endif
