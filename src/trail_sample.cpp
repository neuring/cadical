#include "internal.hpp"
#include<iostream>
#include<fstream>

namespace CaDiCaL {
    static double clamp(double val, double low, double high) {
        if (val < low) return low;
        else if (high < val) return high;
        else return val;
    }

    static ofstream* get_lbd_output_stream() {
        static ofstream *output_file = nullptr;
        if (output_file == nullptr) {
            output_file = new ofstream();
            output_file->open("fuzzy_lbd_error.data");
            *output_file << "fuzzy, expected, error, size" << std::endl;
        }
        return output_file;
    }

    static ofstream* get_polarity_output_stream() {
        static ofstream *output_file = nullptr;
        if (output_file == nullptr) {
            output_file = new ofstream();
            output_file->open("fuzzy_polarity.data");
            *output_file << "fuzzy, sum, size" << std::endl;
        }
        return output_file;
    }

    static void write_new_assignment_reason_ratios(Internal* internal) {
        auto output_file = get_lbd_output_stream();

        bool printed_something = false;

        for (int trail_pos = 0; trail_pos < internal->trail.size(); trail_pos += 1) {
            auto lit = internal->trail[trail_pos];
            auto var = internal->vidx(lit);
            if (var % 100 != 0) continue;


            auto elit = internal->externalize(lit);
            auto evar = internal->external->vidx(elit);

            *output_file << evar << ":" << internal->assignment_reason[var] << ", ";
            printed_something = true;
        }

        if (printed_something)
            *output_file << std::endl;
    }

    void Internal::sample_trail() {
        auto control_iter = this->control.begin();

        for (int trail_pos = 0; trail_pos < this->trail.size(); trail_pos += 1) {
            auto lit = trail[trail_pos];
            auto var = vidx(lit);

            if (control_iter != this->control.end() && lit == this->trail[control_iter->trail]) {
                // lit is decision lit
                UPDATE_AVERAGE(this->assignment_reason[var], 1);
            } else {
                // lit was propagated
                UPDATE_AVERAGE(this->assignment_reason[var], 0);
            }

            UPDATE_AVERAGE(this->assignment_polarity[var], lit > 0 ? 1 : 0);

            if (control_iter != this->control.end() && trail_pos >= control_iter->trail) {
                control_iter++;
            }
        }

        //write_new_assignment_reason_ratios(this);
    }

// fuzzy LBD

    static void write_fuzzy_lbd_data_to_file(double fuzzy_lbd, int expected_lbd, double error, int size) {
        auto output_file = get_lbd_output_stream();

        *output_file << fuzzy_lbd << ", " << expected_lbd << ", " << error << ", " << size << std::endl;
    }

    void Internal::compare_clause_lbd_with_fuzzy_lbd(std::vector<int>& clause, int lbd) {
        return; // Disable comment this in or out if it should be enabled.

        auto fuzzy_lbd = this->calculate_fuzzy_lbd(clause);
        auto error = abs(fuzzy_lbd - lbd);
        write_fuzzy_lbd_data_to_file(fuzzy_lbd, lbd, error,clause.size());
    }

    static void write_fuzzy_polarity_data_to_file(double fuzzy_polarity, double fuzzy_sum, int size) {
        auto output_file = get_polarity_output_stream();

        *output_file << fuzzy_polarity << ", " << fuzzy_sum << ", " << size << std::endl;
    }

    double Internal::calculate_fuzzy_lbd(std::vector<int>& clause) {
        double fuzzy_lbd = 0;
        for (auto lit : clause) {
            fuzzy_lbd += this->assignment_reason[vidx(lit)].value;
        }
        return fuzzy_lbd;
    }

// Stability

    void Internal::compare_polarity_ratio_with_conflict_or_prop_clause(const Clause *clause, bool is_conflict /*either reason or conflict*/) {
        return; // Disable comment this in or out if it should be enabled.
        if (!is_conflict) return;

        double fuzzy_polarity = this->calculate_estimated_conflict_probability(clause);
        double fuzzy_sum = this->calculate_stability_sum(clause);

        write_fuzzy_polarity_data_to_file(fuzzy_polarity, fuzzy_sum, clause->size);
    }

    double _calculate_estimated_conflict_probability(Internal *internal, const int *clause, size_t size) {
        double probability = 1.0;

        for (int i = 0; i < size; i++) {
            auto lit = clause[i];

            auto var_prob = internal->assignment_polarity[internal->vidx(lit)].value;
            assert(-0.001 <= var_prob && var_prob <= 1.001);
            var_prob = clamp(var_prob, 0, 1); // Apparently the cadicals EMA implementation doesn't guarante that its value is between the lowest and highest value provided.

            // Probability that lit is satisfied
            auto lit_prob = lit < 0 ? 1 - var_prob : var_prob;
            assert(0 <= lit_prob && lit_prob <= 1);

            probability *= 1 - lit_prob;
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
            auto var_prob = internal->assignment_polarity[internal->vidx(lit)].value;
            assert(-0.001 <= var_prob && var_prob <= 1.001);
            var_prob = clamp(var_prob, 0, 1); // Apparently the cadicals EMA implementation doesn't guarante that its value is between the lowest and highest value provided.

            // Probability that lit is satisfied
            auto lit_prob = lit < 0 ? 1 - var_prob : var_prob;
            assert(0 <= lit_prob && lit_prob <= 1);

            sum += lit_prob;
        }

        return sum ;
    }

    double Internal::calculate_min_stability_literal(std::vector<int>& clause) {
        assert(clause.size() > 0);
        double min = INFINITY;

        for (int i = 0; i < clause.size(); i++) {
            auto lit = clause[i];
            auto var_stability = this->assignment_polarity[vidx(lit)].value;
            auto lit_stability = lit < 0 ? 1 - var_stability : var_stability;
            if (min > lit_stability) {
                min = lit_stability;
            }
        }

        return min;
    }

    double Internal::calculate_stability_sum(std::vector<int>& clause) {
        return _calculate_stability_sum(this, clause.data(), clause.size());
    }

    double Internal::calculate_stability_sum(const Clause* clause) {
        return _calculate_stability_sum(this, clause->literals, clause->size);
    }

    double Internal::clause_stability(const Clause *clause) {
        double result = 0.0;

        for (int i = 0; i < clause->size; i++) {
            auto lit = clause->literals[i];
            auto var_stability = this->assignment_polarity[vidx(lit)].value;
            result += lit > 0 ? var_stability : 1 - var_stability;
        }

        return result;
    }
}