#include "internal.hpp"
#include<iostream>
#include<fstream>

namespace CaDiCaL {
    static double clamp(double val, double low, double high) {
        if (val < low) return low;
        else if (high < val) return high;
        else return val;
    }

    static ofstream* get_stability_output_stream() {
        static ofstream *output_file = nullptr;
        if (output_file == nullptr) {
            output_file = new ofstream();
            output_file->open("fuzzy_stability.data");
            *output_file << "fuzzy, sum, size" << std::endl;
        }
        return output_file;
    }

    double lit_stability_true(Internal *internal, const int lit) {
        auto lit_prob = lit > 0 ? internal->stability_true [internal->vidx(lit)].value
                                : internal->stability_false[internal->vidx(lit)].value;

        assert(-0.001 <= lit_prob && lit_prob <= 1.001);
        lit_prob = clamp(lit_prob, 0, 1);
        return lit_prob;
    }

    double lit_stability_false(Internal *internal, const int lit) {
        // The probability that the literal is false. Note how if lit is positive we sample from stability_false and reversed.
        auto lit_prob = lit > 0 ? internal->stability_false[internal->vidx(lit)].value
                                : internal->stability_true [internal->vidx(lit)].value;

        assert(-0.001 <= lit_prob && lit_prob <= 1.001);
        lit_prob = clamp(lit_prob, 0, 1);
        return lit_prob;
    }

    double lit_stability_unassigned(Internal * internal, const int lit) {
        double stability_true  = internal->stability_true [internal->vidx(lit)].value;
        double stability_false = internal->stability_false[internal->vidx(lit)].value;

        stability_true  = clamp(stability_true , 0, 1);
        stability_false = clamp(stability_false, 0, 1);
        return 1.0 - stability_true - stability_false;
    }

    double var_stability_polarity(Internal * internal, const int var) {
        double stability_true  = internal->stability_true [internal->vidx(var)].value;
        double stability_false = internal->stability_false[internal->vidx(var)].value;

        if (stability_true + stability_false == 0) return 0.5;
        else return stability_true / (stability_true + stability_false);
    }

    void Internal::sample_trail() {
        // For right now we sample each variable, independent of whether its assigned or not.
        // If the current approach bears fruit, we will optimize it.
        for (int var : this->vars) {
            switch (this->val(var)) {
                case 0: 
                    UPDATE_AVERAGE(this->stability_true[var], 0);
                    UPDATE_AVERAGE(this->stability_false[var], 0);
                    break;
                case 1: 
                    UPDATE_AVERAGE(this->stability_true[var], 1);
                    UPDATE_AVERAGE(this->stability_false[var], 0);
                    break;
                case -1: 
                    UPDATE_AVERAGE(this->stability_true[var], 0);
                    UPDATE_AVERAGE(this->stability_false[var], 1);
                    break;
            }
        }
    }

    double probability_lit_is_false(Internal *internal, const int lit) {
        // The probability that the literal is false. Note how if lit is positive we sample from stability_false and reversed.
        auto lit_prob = lit > 0 ? internal->stability_false[internal->vidx(lit)].value
                                : internal->stability_true [internal->vidx(lit)].value;

        assert(-0.001 <= lit_prob && lit_prob <= 1.001);
        lit_prob = clamp(lit_prob, 0, 1); // Apparently the cadicals EMA implementation doesn't guarante that its value is between the lowest and highest value provided.
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
            //std::cout << "litprob = " << lit_prob << ", res = " << result << std::endl;
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

    double Internal::clause_conflict_heuristic_count_avg(const std::vector<int>& clause) {
        int count = 0;

        for (auto lit : clause) {
            count += probability_lit_is_false(this, lit) >= 0.99 ? 1 : 0;
        }

        return ((double) count) / ((double)clause.size());
    }
}