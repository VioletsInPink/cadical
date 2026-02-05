#include "internal.hpp"

#include <cassert>
#include <limits.h>

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

PalRupTracer::PalRupTracer (Internal *i, File *f, bool b)
    : internal (i), file (f), binary (b)
#ifndef QUIET
      ,
      added (0), deleted (0)
#endif
      ,
      latest_id (0) {
  (void) internal;

  nb_solvers = internal->opts.lratsolvercount;
  solver_modulo_remainder = 0; // (nb_solvers - (internal->opts.lratorigclscount % nb_solvers)) % nb_solvers;
  chain_copy.reserve(128);
}

void PalRupTracer::connect_internal (Internal *i) {
  internal = i;
  file->connect_internal (internal);
  LOG ("PALRUP TRACER connected to internal");
}

PalRupTracer::~PalRupTracer () {
  LOG ("PALRUP TRACER delete");
  delete file;
}

/*------------------------------------------------------------------------*/

inline void PalRupTracer::put_binary_zero () {
  assert (binary);
  assert (file);
  file->put ((unsigned char) 0);
}

inline void PalRupTracer::put_binary_lit (int lit) {
  assert (binary);
  assert (file);
  assert (lit != INT_MIN);
  unsigned idx = abs (lit);
  assert (idx < (1u << 31));
  unsigned x = 2 * idx + (lit < 0);
  unsigned char ch;
  while (x & ~0x7f) {
    ch = (x & 0x7f) | 0x80;
    file->put (ch);
    x >>= 7;
  }
  ch = x;
  file->put (ch);
}

inline void PalRupTracer::put_binary_id (int64_t id) {
  assert (binary);
  assert (file);
#ifndef NDEBUG
  // Unfortunately 'std::numeric_limits<int64_t>::min ()' does not seem to
  // be available for pedantic compilation.
  assert ((uint64_t) id != ((~(uint64_t) 0) >> 1));
#endif
  uint64_t u = (id < 0) ? -id : id;
  assert (u < (((uint64_t) 1) << 63));
  uint64_t x = 2 * u + (id < 0);
  unsigned char ch;
  while (x & ~0x7f) {
    ch = (x & 0x7f) | 0x80;
    file->put (ch);
    x >>= 7;
  }
  ch = x;
  file->put (ch);
}

/*------------------------------------------------------------------------*/

// The following methods are taken and adapted from Michael Dörr's original PalRUP logging implementation,
// integrated in his fork of ImpCheck: https://github.com/MichaelDoerr/impcheck

uint64_t PalRupTracer::plrat_utils_add_offset(uint64_t* hints, int nb_hints) {
    uint64_t max_hint_id = 1;

    // look at hints and translate their id to id + offset
    for (int i = 0; i < nb_hints; ++i) {
        uint64_t hint = hints[i];
        auto it = id_offsets.find(hint);
        if (it != id_offsets.end()) {
          uint64_t current_offset = it->second;
          hint += current_offset;
          assert(hint > 0);
          assert((long)current_offset >= 0);
          assert(hint >= hints[i]);
          //plrat_utils_debug('h', hints[i], hint);
          hints[i] = hint;
        }
        max_hint_id = (max_hint_id < hint) ? hint : max_hint_id;  // find largest hint id
    }

    for (int i = 0; i < nb_hints; ++i) assert(max_hint_id >= hints[i]);

    return max_hint_id;
}

uint64_t PalRupTracer::plrat_utils_get_next_valid_id(const uint64_t old_id, uint64_t* hints, int nb_hints) {

    uint64_t local_offset = offset + solver_modulo_remainder;
    uint64_t new_id = old_id + local_offset;
    uint64_t max_hint_id = plrat_utils_add_offset(hints, nb_hints);

    if (new_id > max_hint_id) {
        //plrat_utils_debug('a', old_id, new_id);
        id_offsets[old_id] = local_offset;
        return new_id;  // no new offset needed
    }

    uint64_t new_offset = 1 + max_hint_id - old_id;
    assert(new_offset + old_id > max_hint_id);

    // offsets have to be a multiple of nb_solvers
    uint64_t temp_rank = (new_offset % nb_solvers);
    // (correct the rank) and do not add nb_solvers if temp_rank is 0
    new_offset += (nb_solvers - temp_rank) % nb_solvers;

    assert((new_offset % nb_solvers) ==  0);
    id_offsets[old_id] = new_offset + solver_modulo_remainder;

    new_id = old_id + new_offset + solver_modulo_remainder;
    //plrat_utils_debug('b', old_id, new_id);

    assert(new_offset > offset);
    offset = new_offset;

    assert(new_id > max_hint_id);
    assert((new_id % nb_solvers) ==  ((old_id + solver_modulo_remainder) % nb_solvers));
    assert(offset % nb_solvers == 0);

    assert(internal->is_locally_produced_lrat_id(old_id) == internal->is_locally_produced_lrat_id(new_id));
    return new_id;
}

// trusted_utils_write_lrat_delete needs hints which are translated.
// clauses that are deleted from the clause table, can be deleted from the offset table too.
// to avoid temporary memory allocation, translating and deleting is done element wise in the same loop.
void PalRupTracer::plrat_utils_translate_and_delete(uint64_t* hints, int nb_hints) {
    for (int i = 0; i < nb_hints; ++i) {
        uint64_t original_id = hints[i];                                       // temp save id
        bool local = internal->is_locally_produced_lrat_id(original_id);
        //plrat_utils_debug('d', original_id, (uint64_t)current_offset);
        auto it = id_offsets.find(original_id);
        if (it != id_offsets.end()) {
          uint64_t current_offset = it->second;  // translate id
          hints[i] = original_id + (uint64_t)current_offset;
          assert(local == internal->is_locally_produced_lrat_id(hints[i]));
          //plrat_utils_debug('D', original_id, hints[i]);
          id_offsets.erase(original_id);
        }
    }
}









void PalRupTracer::lrat_add_clause (uint64_t id, bool,
                                  const vector<int> &clause,
                                  vector<uint64_t> &chain, bool imported) {

  if (!imported) {
    id = plrat_utils_get_next_valid_id(id, chain.data(), chain.size());
    internal->transformed_id = id;

    // sanity check
    if (id <= latest_id) {
      printf("ERROR - added import ID %lu out of order (prev: %lu)!\n", id, latest_id);
      abort();
    }
  }

  if (delete_ids.size ()) {
    plrat_utils_translate_and_delete (delete_ids.data (), delete_ids.size ());
    if (!binary)
      file->put (latest_id), file->put (" ");
    if (binary)
      file->put ('d');
    else
      file->put ("d ");
    for (auto &did : delete_ids) {
      if (binary)
        put_binary_id (did);
      else
        file->put (did), file->put (" ");
    }
    if (binary)
      put_binary_zero ();
    else
      file->put ("0\n");
    delete_ids.clear ();
  }
  if (!imported) latest_id = id;

  if (binary)
    file->put (imported ? 'i' : 'a'), put_binary_id (id);
  else {
    file->put (id);
    if (imported) file->put (" "), file->put ('i');
    file->put (" ");
  }
  for (const auto &external_lit : clause)
    if (binary)
      put_binary_lit (external_lit);
    else
      file->put (external_lit), file->put (' ');

  if (!imported) {
    if (binary)
      put_binary_zero ();
    else
      file->put ("0 ");
    for (const auto &c : chain) {
      assert (c < id); // essential acyclicity property of PalRUP (why we do the ID transformations)
      if (binary)
        put_binary_id (c);
      else
        file->put (c), file->put (' '); // in proof chain, so they get
    }
  }

  if (binary)
    put_binary_zero (); // since cadical has no rat-steps
  else
    file->put ("0\n"); // this is just 2c here
}

void PalRupTracer::lrat_delete_clause (uint64_t id) {
  if (!internal->opts.lratdeletelines) return;
  delete_ids.push_back (id); // pushing off deletion for later
}

/*------------------------------------------------------------------------*/

void PalRupTracer::add_derived_clause (uint64_t id, bool redundant,
                                     const vector<int> &clause,
                                     const vector<uint64_t> &chain) {
  mtx_write.lock ();
  LOG ("LRAT TRACER tracing addition of derived clause");
  chain_copy.insert (chain_copy.end(), chain.begin(), chain.end());
  lrat_add_clause (id, redundant, clause, chain_copy, false);
  chain_copy.clear();
#ifndef QUIET
  added++;
#endif
  mtx_write.unlock ();
}

void PalRupTracer::add_original_clause_with_signature (uint64_t id,
    const vector<int> & clause, const std::vector<uint8_t>& signature) {
  if (internal->is_locally_produced_lrat_id (id)) {
    printf("ERROR: Invalid imported ID %lu for solver %i out of %i!\n", id,
      internal->opts.lratsolverid, internal->opts.lratsolvercount);
    abort();
  }
  mtx_write.lock ();
  // Also remember the imported ID so we can avoid to re-import it as long as it wasn't deleted.
  internal->active_imported_ids.insert(id);
  lrat_add_clause(id, true, clause, chain_copy, true);
  mtx_write.unlock ();
}

void PalRupTracer::delete_clause (uint64_t id, bool, const vector<int> &) {
  if (file->closed ())
    return;
  LOG ("LRAT TRACER tracing deletion of clause");
  lrat_delete_clause (id);
#ifndef QUIET
  deleted++;
#endif
}

void PalRupTracer::begin_proof (uint64_t id) {
  if (file->closed ())
    return;
  LOG ("LRAT TRACER tracing begin of proof");
  latest_id = id;
}

/*------------------------------------------------------------------------*/

bool PalRupTracer::closed () { return file->closed (); }

#ifndef QUIET

void PalRupTracer::print_statistics () {
  uint64_t bytes = file->bytes ();
  uint64_t total = added + deleted;
  MSG ("LRAT %" PRId64 " added clauses %.2f%%", added,
       percent (added, total));
  MSG ("LRAT %" PRId64 " deleted clauses %.2f%%", deleted,
       percent (deleted, total));
  MSG ("LRAT %" PRId64 " bytes (%.2f MB)", bytes,
       bytes / (double) (1 << 20));
}

#endif

void PalRupTracer::close (bool print) {
  assert (!closed ());
  file->close ();
#ifndef QUIET
  if (print) {
    MSG ("LRAT proof file '%s' closed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

void PalRupTracer::flush (bool print) {
  assert (!closed ());
  file->flush ();
#ifndef QUIET
  if (print) {
    MSG ("LRAT proof file '%s' flushed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

void PalRupTracer::stop_asynchronously() {
  mtx_write.lock (); // never gets unlocked!
  if (closed ()) {
    return;
  }
  flush(false);
  close(false);
}

} // namespace CaDiCaL
