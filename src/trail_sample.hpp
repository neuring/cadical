#ifndef _trail_sample_hpp_INCLUDED
#define _trail_sample_hpp_INCLUDED

#include "cema.hpp"

namespace CaDiCaL {

struct CEMACollector {
  CEMA true_stability;
  CEMA false_stability;
  int64_t last_updated;
};

struct StabilityCollector {
  vector<CEMACollector> stability;
  double stability_ema_alpha;  

  // Cache last used (1 - alpha)^k calculation.
  double cached_exp_repetition;
  int cached_repetition;

  StabilityCollector() 
    : stability(), stability_ema_alpha(-1), cached_exp_repetition(1.0), cached_repetition(0)
  {}

  void update_var(int variable, int true_assignment, int false_assignment, int current_epoch) {
    int repetition = current_epoch - this->stability[variable].last_updated;
    assert(repetition >= 0);
    this->stability[variable].last_updated = current_epoch;

    if (repetition > this->cached_repetition) {
      this->cached_exp_repetition *= std::pow(1 - this->stability_ema_alpha, repetition - this->cached_repetition);
      this->cached_repetition = repetition;
    } else if (repetition < this->cached_repetition) {
      this->cached_exp_repetition = std::pow(1 - this->stability_ema_alpha, repetition);
      this->cached_repetition = repetition;
    }

    this->stability[variable] .true_stability.bulk_update(true_assignment,  repetition, this->stability_ema_alpha, this->cached_exp_repetition);
    this->stability[variable].false_stability.bulk_update(false_assignment, repetition, this->stability_ema_alpha, this->cached_exp_repetition);
  }
};

}

#endif
