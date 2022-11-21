#include "internal.hpp"
#include <iostream>
#include <memory>
#include <sstream>

namespace CaDiCaL {

class Heuristic {
    public:
      virtual bool higher_is_better() = 0; // If a clause with a higher heuristic value is considered better than a clause with a lower heuristic value.
      virtual double heuristic_bound() = 0; // Heuristic value that is theoretically the best value, except that no clause can ever achieve this value.
      virtual double eval_clause(Internal*, std::vector<int>&) = 0;

      // is heuristic value a better than heuristic value b.
      virtual bool is_better(double a, double b) {
        if (this->higher_is_better()) {
          return a > b;
        } else {
          return a < b;
        }
      }
};

class SizeHeuristic : public Heuristic {
    public:
      SizeHeuristic() = default;

      virtual bool higher_is_better() { return false; };
      virtual double heuristic_bound() { return 0; };
      virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
          return clause.size();
      }
};

class ProductNormHeuristic : public Heuristic {
    public:
      ProductNormHeuristic() = default;

      virtual bool higher_is_better() { return true; };
      virtual double heuristic_bound() { return 2.0; };
      virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
          return internal->clause_conflict_heuristic_product_norm(clause);
      }
};

class AverageHeuristic : public Heuristic {
  public:
    AverageHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double heuristic_bound() { return 2.0; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_average(clause);
    }
};

class LukasiewieczHeuristic : public Heuristic {
  public:
    LukasiewieczHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double heuristic_bound() { return 2.0; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_lukasiewicz(clause);
    }
};

class MinNormHeuristic : public Heuristic {
  public:
    MinNormHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double heuristic_bound() { return 2.0; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_min(clause);
    }
};

class SecondMinHeuristic : public Heuristic {
  public:
    SecondMinHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double heuristic_bound() { return 2.0; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_second_min(clause);
    }
};

class UnstableLiteralsHeuristic : public Heuristic {
  public:
    UnstableLiteralsHeuristic() = default;

    virtual bool higher_is_better() { return false;};
    virtual double heuristic_bound() { return -1.0; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_unstable_lits(clause);
    }
};

class UnstableLiteralsModifiedHeuristic : public Heuristic {
  public:
    UnstableLiteralsModifiedHeuristic() = default;

    virtual bool higher_is_better() { return false;};
    virtual double heuristic_bound() { return -1.0; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_unstable_lits_minus_stable_lits(clause);
    }
};

std::unique_ptr<Heuristic> get_heuristic_from_code(int code) {
  switch (code) {
    case 0: return std::make_unique<SizeHeuristic>();
    case 1: return std::make_unique<ProductNormHeuristic>();
    case 2: return std::make_unique<AverageHeuristic>();
    case 3: return std::make_unique<LukasiewieczHeuristic>();
    case 4: return std::make_unique<MinNormHeuristic>();
    case 5: return std::make_unique<SecondMinHeuristic>();
    case 6: return std::make_unique<UnstableLiteralsHeuristic>();
    case 7: return std::make_unique<UnstableLiteralsModifiedHeuristic>();
    default: 
      assert(false); 
      return std::make_unique<SizeHeuristic>();
  };
}

bool Internal::importing () {
  return level == 0 && external->learnSource != 0 
      && watching() && external->learnSource->hasNextClause ();
}

struct ClauseWithGlue {
  std::vector<int> clause;
  int glue;
};

struct IndexWithHeuristic {
  size_t clause_idx;
  double heuristic;
};

void add_new_imported_clause(Internal *internal, ClauseWithGlue& imported_clause, bool bc_selection_heuristic) {
    internal->clause = imported_clause.clause;
    Clause * cls_res = internal->new_clause (true, imported_clause.glue);
    cls_res->imported = true;
    cls_res->import_bc_heuristic = bc_selection_heuristic;

    internal->stats.import.imported_clauses += 1;
    internal->stats.import.imported_clauses_bc_selection_heuristic += bc_selection_heuristic ? 1 : 0;

    internal->clause.clear();
    if (internal->proof) internal->proof->add_derived_clause (cls_res);
    assert (internal->watching ());
    internal->watch_clause (cls_res);
}

// Given the clause candidates, this function creates a new vector where each elements has an index to its corresponding clause in the candidates vector.
// The returned vector is sorted using the given heuristic.
std::vector<IndexWithHeuristic> create_index_vector(Internal *internal, std::unique_ptr<Heuristic>& heuristic, std::vector<ClauseWithGlue>& clause_candidates) {

  std::vector<IndexWithHeuristic> ordered_indices(clause_candidates.size());
  for (size_t i = 0; i < ordered_indices.size(); i++) {
    ordered_indices[i].clause_idx = i;
    ordered_indices[i].heuristic = heuristic->eval_clause(internal, clause_candidates[i].clause);
  }

  std::sort(ordered_indices.begin(), ordered_indices.end(), [&heuristic, &clause_candidates](const auto& left, const auto& right) {
    if (left.heuristic < right.heuristic) {
      return !heuristic->higher_is_better();
    } else if (left.heuristic > right.heuristic) {
      return heuristic->higher_is_better();
    } else {
      double left_size = clause_candidates[left.clause_idx].clause.size();
      double right_size = clause_candidates[right.clause_idx].clause.size();
      return left_size < right_size;
    };
  });

  return ordered_indices;
}

std::string clause_to_string(std::vector<int>& clause) {
  std::ostringstream result;

  for (auto lit : clause) {
    result << lit << ", ";
  }
  return result.str();
}

void import_useful_clauses(int& res, Internal *internal, std::vector<ClauseWithGlue> clause_candidates, size_t already_imported) {

  auto selection_heuristic = get_heuristic_from_code(internal->opts.importselectionheuristic);
  std::vector<IndexWithHeuristic> selection_heuristic_order = create_index_vector(internal, selection_heuristic, clause_candidates);
  std::vector<bool> selected_clauses(clause_candidates.size());
  double importratio = internal->opts.importpercent / 100.0;
  double importselectionthreshold = internal->opts.importselectionthreshold / 100.0;
  int imported_clauses = 0;

  for (auto selected_clause : selection_heuristic_order) {
    if ((double) already_imported / (double) internal->opts.importbuffersize >= importratio) {
      break; // We already reached the import limit.
    }

    if (selection_heuristic->is_better(selected_clause.heuristic, importselectionthreshold)) {
      auto& selected = clause_candidates[selected_clause.clause_idx];
      add_new_imported_clause(internal, selected, true);

      already_imported += selected.clause.size() + 1;
      selected_clauses[selected_clause.clause_idx] = true;
      imported_clauses += 1;
    }
  }

  int imported_bc_selection_heuristic = imported_clauses;

  // Add the remaining clauses using the fallback heuristic until we reach the limit importpercent.
  auto fallback_heuristic = get_heuristic_from_code(internal->opts.importfallbackheuristic);
  std::vector<IndexWithHeuristic> fallback_heuristic_order = create_index_vector(internal, fallback_heuristic, clause_candidates);

  //for (auto cls : fallback_heuristic_order) {
  //  std::cout << "Fallback clause : " << clause_to_string(clause_candidates[cls.clause_idx].clause) << std::endl;
  //}

  for (auto selected_clause : fallback_heuristic_order) {
    if ((double) already_imported / (double) internal->opts.importbuffersize >= importratio) {
      break; // We already reached the import limit.
    }

    if (selected_clauses[selected_clause.clause_idx] == false) { // Clause hasn't been selected by the selection heuristic.
      auto& selected = clause_candidates[selected_clause.clause_idx];
      add_new_imported_clause(internal, selected, false);

      already_imported += selected.clause.size() + 1;
      selected_clauses[selected_clause.clause_idx] = true;
      imported_clauses += 1;
    }
  }

  // Check if SAT or UNSAT was found
  if (internal->unsat) {
    res = 20;
    return;
  }
  if (internal->satisfied ()) {
    res = 10;
    return;
  }

  std::cout << "imported " << imported_clauses << " (selected " << imported_bc_selection_heuristic << ")" << " of " << clause_candidates.size() << "(" << already_imported << ")" << std::endl;
}

void Internal::import_redundant_clauses (int& res) {
  if (external->learnSource == 0) return;
  if (res != 0) return;

  // Store clauses > 2 and select the 'best' ones afterwards.
  std::vector<ClauseWithGlue> clause_candidates;
  // How much data was already imported (in literals and zero terminators)
  size_t already_imported = 0;

  // Import external clauses.
  while (external->learnSource->hasNextClause ()) {

    // Fetch pointer to 1st literal and size of the clause (plus glue)
    auto cls = external->learnSource->getNextClause ();
    size_t size = cls.size ();
    //printf("Import clause of size %lu\n", size);
    assert (size > 0);
    int unitLit = size == 1 ? cls[0] : 0;
    assert (clause.empty ());

    if (unitLit == 0) {
      // Learn non-unit clause

      // Glue int at the front
      int glue = cls[0];
      assert (glue > 0);

      // Analyze clause literals
      bool addClause = true;
      for (size_t i = 1; i < size; i++) {

        int elit = cls[i];
        assert (elit != 0);

        if (external->marked (external->witness, elit)) {
          // Literal marked as witness: Cannot import
          addClause = false; break;
        }

        int ilit = external->internalize(elit);

        auto& f = flags (ilit);
        if (f.eliminated ()) {
          // Literal has been eliminated: do not add this clause.
          addClause = false; break;
        } else if (f.fixed ()) {
          // Literal is fixed
          if (val (ilit) == 1) {
            // TRUE: Clause can be omitted.
            addClause = false; break;
          } // else: FALSE - literal can be omitted.
        } else {
          // Active, pure, or substituted: Can treat literal normally.
          clause.push_back (ilit);
          unitLit = elit;
        }
      }

      if (!addClause) {
        //printf("Discard clause\n");
        clause.clear ();
        continue;
      }

      // Handle clause of size >= 2 being learnt
      // (unit clauses are handled below)
      if (clause.size () >= 2) {
        clause_candidates.push_back({clause, glue});
        external->check_learned_clause ();
        unitLit = 0;
      }

      clause.clear ();
    }

    // Try to learn unit clause
    if (unitLit != 0) {
      bool add = true;
      if (external->marked (external->witness, unitLit)) {
        // Do not learn unit clause if marked as witness
        continue;
      }
      int ilit = external->internalize (unitLit);
      auto& f = flags(ilit);
      if (f.eliminated () || f.substituted ()) {
        // Do not import eliminated or substituted literal
        continue;
      }
      // Do not import units which are already fixed
      if (f.status == Flags::FIXED) continue;
      // Actually add the unit clause
      if (add) assign_original_unit (ilit);
      already_imported += 2; // imported one literals and its zero terminator.
    }

    // Stop importing if SAT or UNSAT was found
    if (unsat) {
      res = 20;
      return;
    }
    if (satisfied ()) {
      res = 10;
      return;
    }
  }

  import_useful_clauses(res, this, clause_candidates, already_imported);
}

}