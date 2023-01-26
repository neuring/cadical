#include "internal.hpp"
#include<cmath>

namespace CaDiCaL {

double CEMA::value() {
  return this->exponential_part + this->cumulative_part;
}

void CEMA::bulk_update(double next_values, int repetition, double alpha) {
  double exp_repetition = std::pow(1 - alpha, repetition);

  this->bulk_update(next_values, repetition, alpha, exp_repetition);
}

void CEMA::bulk_update(double next_values, int repetition, double alpha, double exp_repetition) {
  (void) alpha;
  double new_exponential_part = next_values + (this->exponential_part - next_values) * exp_repetition;

  if (this->cumulative_factor || this->cumulative_part) {
    double new_cumulative_part = exp_repetition * (this->cumulative_part + repetition * (this->cumulative_factor * next_values - this->cumulative_part) / (this->time + repetition));
    this->cumulative_part = new_cumulative_part;
  }

  this->exponential_part = new_exponential_part;
  this->time += repetition;
  this->cumulative_factor *= exp_repetition;
}

}