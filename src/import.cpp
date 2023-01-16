#include "internal.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <numeric>

namespace CaDiCaL {

namespace HeuristicCode {
  enum HeuristicCode {
    NO_HEURISTIC = -1,
    SIZE = 0,
    PRODUCT_NORM = 1,
    AVERAGE = 2,
    LUKASIEWIECZ = 3,
    MIN_NORM = 4,
    SECOND_MIN = 5,
    UNSTABLE_LITERALS = 6,
    UNSTABLE_LITERALS_MOD = 7,
    LITERAL_SCORE_SUM = 8,
  };
}

class Heuristic {
    public:
      virtual bool higher_is_better() = 0; // If a clause with a higher heuristic value is considered better than a clause with a lower heuristic value.
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
      virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
          (void) internal;
          return clause.size();
      }
};

class ProductNormHeuristic : public Heuristic {
    public:
      ProductNormHeuristic() = default;

      virtual bool higher_is_better() { return true; };
      virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
          return internal->clause_conflict_heuristic_product_norm(clause);
      }
};

class AverageHeuristic : public Heuristic {
  public:
    AverageHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_average(clause);
    }
};

class LukasiewieczHeuristic : public Heuristic {
  public:
    LukasiewieczHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_lukasiewicz(clause);
    }
};

class MinNormHeuristic : public Heuristic {
  public:
    MinNormHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_min(clause);
    }
};

class SecondMinHeuristic : public Heuristic {
  public:
    SecondMinHeuristic() = default;

    virtual bool higher_is_better() { return true; };
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_second_min(clause);
    }
};

class UnstableLiteralsHeuristic : public Heuristic {
  public:
    UnstableLiteralsHeuristic() = default;

    virtual bool higher_is_better() { return false;};
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_unstable_lits(clause);
    }
};

class GeneralizedUnstableLiteralsHeuristic : public Heuristic {
  public:
    GeneralizedUnstableLiteralsHeuristic() = default;

    virtual bool higher_is_better() { return false;};
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_generalized_unstable_lits(clause);
    }
};

class LiteralScoreSum : public Heuristic {
  public:
    LiteralScoreSum() = default;

    virtual bool higher_is_better() { return false;};
    virtual double eval_clause(Internal* internal, std::vector<int>& clause) {
        return internal->clause_conflict_heuristic_literal_score_sum(clause);
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
    case 7: return std::make_unique<GeneralizedUnstableLiteralsHeuristic>();
    case 8: return std::make_unique<LiteralScoreSum>();
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
  size_t index;
  double heuristic;
};

void add_new_imported_clause(Internal *internal, ClauseWithGlue& imported_clause) {
    internal->clause = imported_clause.clause;
    Clause * cls_res = internal->new_clause (true, imported_clause.glue);
    cls_res->imported = true;

    internal->stats.import.imported_clauses += 1;

    internal->clause.clear();
    if (internal->proof) internal->proof->add_derived_clause (cls_res);
    assert (internal->watching ());
    internal->watch_clause (cls_res);
}

std::string clause_to_string(std::vector<int>& clause) {
  std::ostringstream result;

  for (auto lit : clause) {
    result << lit << ", ";
  }
  return result.str();
}

void import_useful_clauses(int& res, Internal *internal, std::vector<ClauseWithGlue> clause_candidates, size_t already_imported) {
  int heuristic_code = internal->opts.importheuristic;
  std::unique_ptr<Heuristic> heuristic = get_heuristic_from_code(heuristic_code);

  // Evaluate each imported clause candidate with the specified heuristic.
  std::vector<IndexWithHeuristic> evaluated_clauses;
  for (size_t idx = 0; idx < clause_candidates.size(); idx += 1) {
    double clause_heuristic = heuristic->eval_clause(internal, clause_candidates[idx].clause);
    evaluated_clauses.push_back({ idx, clause_heuristic });
  }

  // Order them from best to worst.
  std::sort(evaluated_clauses.begin(), evaluated_clauses.end(), [&heuristic](const auto& a, const auto& b) {
    return heuristic->higher_is_better() ? a.heuristic >= b.heuristic : a.heuristic < b.heuristic;
  });

  // Determine how many literals can be imported.
  double import_percent = internal->opts.importpercent / 100.0;
  // We start with already_imported because unit clauses don't appear in the clause_candidates vector but should be considered in the literal limit.
  int total_nb_literals = std::accumulate(clause_candidates.begin(), clause_candidates.end(), already_imported, [](int sum, const auto& clause) {
    return sum + clause.clause.size() + 1; // Plus one zero terminator
  });
  size_t imported_literal_limit = import_percent * total_nb_literals;

  // Import the best clauses until literal limit is reached.
  for (auto cls_idx : evaluated_clauses) {
    if (already_imported >= imported_literal_limit) {
      break;
    }

    auto& candidate = clause_candidates[cls_idx.index];

    add_new_imported_clause(internal, candidate);
    already_imported += candidate.clause.size() + 1;
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