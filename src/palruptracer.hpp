#ifndef _palruptracer_h_INCLUDED
#define _palruptracer_h_INCLUDED

#include <mutex>
#include <vector>
#include "file.hpp"
#include "tracer.hpp"
#include "tsl/robin_map.h"

using namespace std;

namespace CaDiCaL {

class PalRupTracer : public FileTracer {

  Internal *internal;
  File *file;
  bool binary;

#ifndef QUIET
  int64_t added, deleted;
#endif
  int64_t latest_id;
  vector<int64_t> delete_ids;

  std::mutex mtx_write;

  uint64_t nb_solvers;
  uint64_t solver_modulo_remainder;
  int64_t offset = 0;
  tsl::robin_map<int64_t, int64_t> id_offsets;

  std::vector<int64_t> chain_copy;

  void put_binary_zero ();
  void put_binary_lit (int external_lit);
  void put_binary_id (int64_t id);

  // support LRAT
  void lrat_add_clause (int64_t, bool, const vector<int> &,
                        vector<int64_t> &, bool imported);
  void lrat_delete_clause (int64_t);

  int64_t plrat_utils_add_offset(int64_t* hints, int nb_hints);
  int64_t plrat_utils_get_next_valid_id(const int64_t old_id, int64_t* hints, int nb_hints);
  void plrat_utils_translate_and_delete(int64_t* hints, int nb_hints);

public:
  // own and delete 'file'
  PalRupTracer (Internal *, File *file, bool binary);
  ~PalRupTracer ();

  void connect_internal (Internal *i) override;
  void begin_proof (int64_t) override;

  void stop_asynchronously () override;

  void add_original_clause (int64_t, bool, const vector<int> &,
                            bool = false) override {} // skip

  void add_derived_clause (int64_t, bool, int, const vector<int> &,
                           const vector<int64_t> &) override;

  void add_original_clause_with_signature (int64_t id, const vector<int> & clause, const vector<uint8_t>& signature) override;

  void delete_clause (int64_t, bool, const vector<int> &) override;

  void finalize_clause (int64_t, const vector<int> &) override {} // skip

  void report_status (int, int64_t) override {} // skip

#ifndef QUIET
  void print_statistics ();
#endif
  bool closed () override;
  void close (bool) override;
  void flush (bool) override;
};

} // namespace CaDiCaL

#endif
