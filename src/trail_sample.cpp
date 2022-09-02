#include "internal.hpp"
#include<iostream>
#include<fstream>

namespace CaDiCaL {
    static ofstream* get_output_stream() {
        static ofstream *output_file = nullptr;
        if (output_file == nullptr) {
            output_file = new ofstream();
            output_file->open("fuzzy_lbd_error.data");
        }

        return output_file;
    }

    static void write_to_file(double fuzzy_lbd, int expected_lbd, double error, int size) {
        auto output_file = get_output_stream();

        *output_file << fuzzy_lbd << ", " << expected_lbd << ", " << error << ", " << size << std::endl;
    }

    static void write_new_assignment_reason_ratios(Internal* internal) {
        auto output_file = get_output_stream();

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

            if (control_iter != this->control.end() && trail_pos >= control_iter->trail) {
                control_iter++;
            }
        }

        //write_new_assignment_reason_ratios(this);
    }

    void Internal::compare_clause_lbd_with_fuzzy_lbd(std::vector<int>& clause, int lbd) {
        double fuzzy_lbd = 0;
        for (auto lit : clause) {
            fuzzy_lbd += this->assignment_reason[vidx(lit)].value;
        }

        auto error = abs(fuzzy_lbd - lbd);
        write_to_file(fuzzy_lbd, lbd, error,clause.size());
    }
}