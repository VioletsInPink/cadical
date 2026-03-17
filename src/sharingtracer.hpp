#ifndef _sharingtracer_h_INCLUDED
#define _sharingtracer_h_INCLUDED

#include "file.hpp"
#include "onthefly_checking.hpp"
#include "tracer.hpp"

class FileTracer;

namespace CaDiCaL {

class SharingTracer : public FileTracer {

  Internal *internal;
  LratCallbackProduceClause cb_produce;

public:
  SharingTracer (Internal *, LratCallbackProduceClause cb_produce);
  ~SharingTracer ();

  // proof section:
  void add_derived_clause (int64_t, bool, int, const std::vector<int> &,
                           const std::vector<int64_t> &) override;
  void add_original_clause_with_signature (int64_t, const std::vector<int> &, const std::vector<uint8_t>&) override {}
  void add_assumption_clause (int64_t, const std::vector<int> &,
                              const std::vector<int64_t> &) override {}
  void weaken_minus (int64_t, const std::vector<int> &) override {}
  void delete_clause (int64_t, bool, const std::vector<int> &) override {}
  void add_original_clause (int64_t, bool, const std::vector<int> &,
                            bool = false) override {}
  void report_status (int, int64_t) override {}
  void conclude_sat (const std::vector<int> &) override {}
  void conclude_unsat (ConclusionType, const std::vector<int64_t> &) override {}
  void conclude_unknown (const std::vector<int> &) override {}

  void solve_query () override {}
  void add_assumption (int) override {}
  void reset_assumptions () override {}

  // skip
  void begin_proof (int64_t) override {}
  void finalize_clause (int64_t, const std::vector<int> &) override {}
  void strengthen (int64_t) override {}
  void add_constraint (const std::vector<int> &) override {}

  // logging and file io
  void connect_internal (Internal *i) override;

#ifndef QUIET
  void print_statistics ();
#endif
  bool closed () override {return false;}
  void close (bool) override {}
  void flush (bool) override {}
};

} // namespace CaDiCaL

#endif
