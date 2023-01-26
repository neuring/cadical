#include "internal.hpp"
#include<iostream>
#include<fstream>
#include "ema.hpp"

namespace CaDiCaL {
    static double clamp(double val, double low, double high) {
        if (val < low) return low;
        else if (high < val) return high;
        else return val;
    }

    void Internal::update_stability(int var) {
        var = this->vidx(var);
        int conflicts_since_last_update = this->stats.conflicts - this->stability[var].last_updated;
        if (conflicts_since_last_update == 0) return;

        this->stability[var].last_updated = this->stats.conflicts;

        var = this->vidx(var);
        switch (this->vals[var]) {
            case 0: 
                this->stability[var].true_stability .bulk_update(0, conflicts_since_last_update, this->stability_ema_alpha);
                this->stability[var].false_stability.bulk_update(0, conflicts_since_last_update, this->stability_ema_alpha);
                this->stability_collector.update_var(var, 0, 0, conflicts_since_last_update);
                for (int i = 0; i < conflicts_since_last_update; i += 1) {
                    UPDATE_AVERAGE(this->ema_true[var], 0);
                    UPDATE_AVERAGE(this->ema_false[var], 0);
                }
                break;
            case 1: 
                this->stability[var].true_stability .bulk_update(1, conflicts_since_last_update, this->stability_ema_alpha);
                this->stability[var].false_stability.bulk_update(0, conflicts_since_last_update, this->stability_ema_alpha);
                this->stability_collector.update_var(var, 1, 0, conflicts_since_last_update);
                for (int i = 0; i < conflicts_since_last_update; i += 1) {
                    UPDATE_AVERAGE(this->ema_true[var], 1);
                    UPDATE_AVERAGE(this->ema_false[var], 0);
                }
                break;
            case -1: 
                this->stability[var].true_stability .bulk_update(0, conflicts_since_last_update, this->stability_ema_alpha);
                this->stability[var].false_stability.bulk_update(1, conflicts_since_last_update, this->stability_ema_alpha);
                this->stability_collector.update_var(var, 0, 1, conflicts_since_last_update);
                for (int i = 0; i < conflicts_since_last_update; i += 1) {
                    UPDATE_AVERAGE(this->ema_true[var], 0);
                    UPDATE_AVERAGE(this->ema_false[var], 1);
                }
                break;
            default: assert(false); // unreacheable
        }

        // TODO: remove compare old and new stabilty values for equality.
        double true_old = this->stability[var].true_stability.value();
        double true_new = this->stability_collector.stability[var].true_stability.value();
        double false_old = this->stability[var].false_stability.value();
        double false_new = this->stability_collector.stability[var].false_stability.value();
        double true_ema = this->ema_true[var].value;
        double false_ema = this->ema_false[var].value;

        bool err = (std::abs(true_ema - true_new) >= 1e-9);
        std::cout << var << " (" << (int)this->vals[var] << " : " << conflicts_since_last_update << ")" << "true: " << true_ema << " != " << true_new << " " << (err ? "ERROR" : "") << std::endl;
        //std::cout << "exp part: " << this->stability[var].true_stability.exponential_part << ", cum part: " << this->stability[var].true_stability.cumulative_part << std::endl;

        //if (std::abs(false_old - false_new) >= 1e-9) {
        //    std::cout << var << "false: " << false_old << " != " << false_new << std::endl;
        //}
    }

    void Internal::update_stability_all_variables() {
        for (int var : this->vars) {
            this->update_stability(var);
        }
    }

    double probability_lit_is_false(Internal *internal, const int lit) {
        // The probability that the literal is false. Note how if lit is positive we sample from stability_false and reversed.
        auto lit_prob = lit > 0 ? internal->stability[internal->vidx(lit)].false_stability.value()
                                : internal->stability[internal->vidx(lit)].true_stability.value();

        assert(-0.001 <= lit_prob && lit_prob <= 1.001);
        lit_prob = clamp(lit_prob, 0, 1);
        return lit_prob;
    }

    double probability_lit_is_true(Internal *internal, const int lit) {
        auto lit_prob = lit > 0 ? internal->stability[internal->vidx(lit)].true_stability.value()
                                : internal->stability[internal->vidx(lit)].false_stability.value();

        assert(-0.001 <= lit_prob && lit_prob <= 1.001);
        lit_prob = clamp(lit_prob, 0, 1);
        return lit_prob;
    }

    double probability_lit_is_unassigned(Internal *internal, const int lit) {
        auto lit_prob = 1.0 - internal->stability[internal->vidx(lit)].false_stability.value() - internal->stability[internal->vidx(lit)].true_stability.value();

        assert(-0.001 <= lit_prob && lit_prob <= 1.001);
        lit_prob = clamp(lit_prob, 0, 1);
        return lit_prob;
    }

    double Internal::clause_conflict_heuristic_average(const std::vector<int>& clause) {
        double sum = 0;

        for (auto lit : clause) {
            sum += probability_lit_is_false(this, lit);
        }

        return sum / clause.size();
    }

    double Internal::clause_conflict_heuristic_lukasiewicz(const std::vector<int>& clause) {
        double result = 1.0;

        for (auto lit : clause) {
            auto lit_prob = probability_lit_is_false(this, lit);
            result = std::max(result + lit_prob - 1, 0.0);
        }

        return result;
    }

    double Internal::clause_conflict_heuristic_product_norm(std::vector<int>& clause) {
        double probability = 1.0;

        for (auto lit : clause) {
            probability *= probability_lit_is_false(internal, lit);
        }

        return probability;
    }

    double Internal::clause_conflict_heuristic_min(const std::vector<int>& clause) {
        double result = 1.0;

        for (auto lit : clause) {
            result = std::min(result, probability_lit_is_false(internal, lit));
        }

        return result;
    }

    double Internal::clause_conflict_heuristic_second_min(const std::vector<int>& clause) {
        double min = 1.0;
        double snd_min = 1.0;

        for (auto lit : clause) {
            auto prob = probability_lit_is_false(internal, lit);

            if (prob < min) {
                snd_min = min;
                min = prob;
            } else if (prob < snd_min) {
                snd_min = prob;
            }
        }

        return snd_min;
    }

    bool Internal::is_lit_stable_false(const int lit) {
        double threshold = ((double) this->opts.stabilitythreshold) / 100.0;
        return probability_lit_is_false(internal, lit) > threshold;
    }

    bool Internal::is_lit_stable_true(const int lit) {
        double threshold = ((double) this->opts.stabilitythreshold) / 100.0;
        return probability_lit_is_true(internal, lit) > threshold;
    }

    int Internal::clause_conflict_heuristic_unstable_lits(const std::vector<int>& clause) {
        int stable_lits = 0;

        for (auto lit : clause) {
            if (this->is_lit_stable_false(lit)) {
                stable_lits += 1;
            }
        }

        return clause.size() - stable_lits;
    }

    double Internal::clause_conflict_heuristic_generalized_unstable_lits(const std::vector<int>& clause) {
        double true_penalty = this->opts.trueliteralpenalty / 100.0;
        double result = 0.0;

        for (auto lit : clause) {
            if (this->is_lit_stable_false(lit)) {
                
            } else if (this->is_lit_stable_true(lit)) {
                result += true_penalty;
            } else {
                result += 1; // Literal is mostly unassigned
            }
        }

        return result;
    }

    double Internal::clause_conflict_heuristic_literal_score_sum(const std::vector<int>& clause) {
        double true_penalty = this->opts.trueliteralpenalty / 100.0;

        double result = 0.0;

        for (auto lit : clause) {
            double lit_unass = probability_lit_is_unassigned(this, lit);
            double lit_true = probability_lit_is_true(this, lit);

            // This is a simplified version of the following calculation:
            // lit_unass + (1 - lit_unass) * (lit_true / (lit_false + lit_true)) * true_penalty;
            double lit_score = lit_unass + lit_true * true_penalty;
            result += lit_score;
        }

        return result;
    }
}