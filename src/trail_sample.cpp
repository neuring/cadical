#include "internal.hpp"
#include<iostream>
#include<fstream>

namespace CaDiCaL {
    static double clamp(double val, double low, double high) {
        if (val < low) return low;
        else if (high < val) return high;
        else return val;
    }

    static ofstream* get_polarity_output_stream() {
        static ofstream *output_file = nullptr;
        if (output_file == nullptr) {
            output_file = new ofstream();
            output_file->open("fuzzy_stability.data");
            *output_file << "fuzzy, sum, size" << std::endl;
        }
        return output_file;
    }

    void Internal::sample_trail() {
        auto control_iter = this->control.begin();

        // For right now we sample each variable, independent of whether its assigned or not.
        // If the current approach bears fruit, we will optimize it.
        for (int var : this->vars) {
            switch (this->val(var)) {
                case 0: 
                    UPDATE_AVERAGE(this->stability_true[var], 0);
                    UPDATE_AVERAGE(this->stability_false[var], 0);
                    break;
                case 1: 
                    UPDATE_AVERAGE(this->stability_true[var], 0);
                    UPDATE_AVERAGE(this->stability_false[var], 0);
                    break;
                case -1: 
                    UPDATE_AVERAGE(this->stability_true[var], 0);
                    UPDATE_AVERAGE(this->stability_false[var], 0);
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
        }

        return result;
    }

    double _calculate_estimated_conflict_probability(Internal *internal, const int *clause, size_t size) {
        double probability = 1.0;

        for (int i = 0; i < size; i++) {
            auto lit = clause[i];

            probability *= probability_lit_is_false(internal, lit);
        }

        return probability;
    }

    double Internal::calculate_estimated_conflict_probability(std::vector<int>& clause) {
        return _calculate_estimated_conflict_probability(this, clause.data(), clause.size());
    }

    double Internal::calculate_estimated_conflict_probability(const Clause *clause) {
        return _calculate_estimated_conflict_probability(this, clause->literals, clause->size);
    }

    static double _calculate_stability_sum(Internal *internal, const int *clause, size_t size) {
        double sum = 1.0;

        for (int i = 0; i < size; i++) {
            auto lit = clause[i];
            auto lit_prob = probability_lit_is_false(internal, lit);

            sum += lit_prob;
        }

        return sum ;
    }

    double Internal::calculate_stability_sum(std::vector<int>& clause) {
        return _calculate_stability_sum(this, clause.data(), clause.size());
    }

    double Internal::calculate_stability_sum(const Clause* clause) {
        return _calculate_stability_sum(this, clause->literals, clause->size);
    }
}