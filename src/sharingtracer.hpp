#ifndef _sharingtracer_h_INCLUDED
#define _sharingtracer_h_INCLUDED

#include "file.hpp"
#include "onthefly_checking.hpp"
#include "tracer.hpp"

#include <vector>
using namespace std;

class FileTracer;

namespace CaDiCaL {

class SharingTracer : public FileTracer {

  Internal *internal;
  LratCallbackProduceClause cb_produce;

public:
  SharingTracer (Internal *, LratCallbackProduceClause cb_produce);
  ~SharingTracer ();

  // proof section:
  void add_derived_clause (uint64_t, bool, const vector<int> &,
                           const vector<uint64_t> &) override;
  void add_original_clause_with_signature (uint64_t, const vector<int> &, const vector<uint8_t>&) override {}
  void add_assumption_clause (uint64_t, const vector<int> &,
                              const vector<uint64_t> &) override {}
  void weaken_minus (uint64_t, const vector<int> &) override {}
  void delete_clause (uint64_t, bool, const vector<int> &) override {}
  void add_original_clause (uint64_t, bool, const vector<int> &,
                            bool = false) override {}
  void report_status (int, uint64_t) override {}
  void conclude_sat (const vector<int> &) override {}
  void conclude_unsat (ConclusionType, const vector<uint64_t> &) override {}
  void conclude_unknown (const vector<int> &) override {}

  void solve_query () override {}
  void add_assumption (int) override {}
  void reset_assumptions () override {}

  // skip
  void begin_proof (uint64_t) override {}
  void finalize_clause (uint64_t, const vector<int> &) override {}
  void strengthen (uint64_t) override {}
  void add_constraint (const vector<int> &) override {}

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
