#include "internal.hpp"
#include<cmath>

namespace CaDiCaL {

double CEMA::value() {
  return this->exponential_part + this->cumulative_part;
}

void CEMA::update(double next_value, double alpha) {
  double new_exponential_part = alpha * (next_value - this->exponential_part) + this->exponential_part;

  if (this->cumulative_part) {
    double new_cumulative_part = (1 - alpha) * (this->cumulative_part + (this->cumulative_factor * next_value - this->cumulative_part) / (this->time + 2));
    this->cumulative_part = new_cumulative_part;
  }

  this->exponential_part = new_exponential_part;
  this->time += 1;
  this->cumulative_factor *= (1 - alpha);
}

  void CEMA::bulk_update(double next_values, int repetition, double alpha) {
    double exp_repetition = std::pow(1 - alpha, repetition);

    double new_exponential_part = next_values + (this->exponential_part - next_values) * exp_repetition + 1;

    if (this->cumulative_part) {
      double new_cumulative_part = exp_repetition * (this->cumulative_part + repetition * (this->cumulative_factor * next_values - this->cumulative_part) / (this->time + repetition + 1));
      this->cumulative_part = new_cumulative_part;
    }

    this->exponential_part = new_exponential_part;

    this->time += repetition;
    this->cumulative_factor *= exp_repetition;
  }

}
