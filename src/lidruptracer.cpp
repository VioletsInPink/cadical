#include "lidruptracer.hpp"
#include "internal.hpp"
#include "tracer.hpp"
#include <cstdio>
#include <vector>

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

LidrupTracer::LidrupTracer (Internal *i, File *f, bool b)
    : internal (i), file (f), binary (b), num_clauses (0), size_clauses (0),
      clauses (0), last_hash (0), last_id (0), last_clause (0)
#ifndef QUIET
      ,
      added (0), deleted (0), weakened (0), restore (0), original (0),
      solved (0), batched (0)
#endif
{
  (void) internal;

  // Initialize random number table for hash function.
  //
  Random random (42);
  for (unsigned n = 0; n < num_nonces; n++) {
    uint64_t nonce = random.next ();
    if (!(nonce & 1))
      nonce++;
    assert (nonce), assert (nonce & 1);
    nonces[n] = nonce;
  }
#ifndef NDEBUG
  binary = b;
#else
  (void) b;
#endif
  piping = file->piping ();
}

LidrupTracer::LidrupTracer(Internal *i,
    LratCallbackProduceClause cb_produce, LratCallbackImportClause cb_import,
      LratCallbackDeleteClauses cb_delete, LratCallbackConcludeUnsat cb_conclude)
    : internal (i), file (0), binary (false), num_clauses (0), size_clauses (0),
      clauses (0), last_hash (0), last_id (0), last_clause (0)
#ifndef QUIET
      ,
      added (0), deleted (0)
#endif
{
  (void) internal;

  // Initialize random number table for hash function.
  //
  Random random (42);
  for (unsigned n = 0; n < num_nonces; n++) {
    uint64_t nonce = random.next ();
    if (!(nonce & 1))
      nonce++;
    assert (nonce), assert (nonce & 1);
    nonces[n] = nonce;
  }
  piping = false;
  callbacks = true;
  this->cb_produce = cb_produce;
  this->cb_import = cb_import;
  this->cb_delete = cb_delete;
  this->cb_conclude = cb_conclude;
}

void LidrupTracer::connect_internal (Internal *i) {
  internal = i;
  if (!callbacks) file->connect_internal (internal);
  LOG ("LIDRUP TRACER connected to internal");
}

LidrupTracer::~LidrupTracer () {
  LOG ("LIDRUP TRACER delete");
  if (!callbacks) delete file;
  for (size_t i = 0; i < size_clauses; i++)
    for (LidrupClause *c = clauses[i], *next; c; c = next)
      next = c->next, delete_clause (c);
  delete[] clauses;
}

/*------------------------------------------------------------------------*/

void LidrupTracer::enlarge_clauses () {
  assert (num_clauses == size_clauses);
  const uint64_t new_size_clauses = size_clauses ? 2 * size_clauses : 1;
  LOG ("LIDRUP Tracer enlarging clauses of tracer from %" PRIu64
       " to %" PRIu64,
       (uint64_t) size_clauses, (uint64_t) new_size_clauses);
  LidrupClause **new_clauses;
  new_clauses = new LidrupClause *[new_size_clauses];
  clear_n (new_clauses, new_size_clauses);
  for (uint64_t i = 0; i < size_clauses; i++) {
    for (LidrupClause *c = clauses[i], *next; c; c = next) {
      next = c->next;
      const uint64_t h = reduce_hash (c->hash, new_size_clauses);
      c->next = new_clauses[h];
      new_clauses[h] = c;
    }
  }
  delete[] clauses;
  clauses = new_clauses;
  size_clauses = new_size_clauses;
}

LidrupClause *LidrupTracer::new_clause () {
  LidrupClause *res = new LidrupClause;
  res->next = 0;
  res->hash = last_hash;
  res->id = last_id;
  for (const auto &id : imported_chain) {
    res->chain.push_back (id);
  }
  for (const auto &lit : imported_clause) {
    res->literals.push_back (lit);
  }
  last_clause = res;
  num_clauses++;
  return res;
}

void LidrupTracer::delete_clause (LidrupClause *c) {
  assert (c);
  num_clauses--;
  delete c;
}

uint64_t LidrupTracer::reduce_hash (uint64_t hash, uint64_t size) {
  assert (size > 0);
  unsigned shift = 32;
  uint64_t res = hash;
  while ((((uint64_t) 1) << shift) > size) {
    res ^= res >> shift;
    shift >>= 1;
  }
  res &= size - 1;
  assert (res < size);
  return res;
}

uint64_t LidrupTracer::compute_hash (const int64_t id) {
  assert (id > 0);
  unsigned j = id % num_nonces;
  uint64_t tmp = nonces[j] * (uint64_t) id;
  return last_hash = tmp;
}

bool LidrupTracer::find_and_delete (const int64_t id) {
  if (!num_clauses)
    return false;
  LidrupClause **res = 0, *c;
  const uint64_t hash = compute_hash (id);
  const uint64_t h = reduce_hash (hash, size_clauses);
  for (res = clauses + h; (c = *res); res = &c->next) {
    if (c->hash == hash && c->id == id) {
      break;
    }
    if (!c->next)
      return false;
  }
  if (!c)
    return false;
  assert (c && res);
  *res = c->next;
  for (auto &lit : c->literals) {
    imported_clause.push_back (lit);
  }
  for (auto &cid : c->chain) {
    imported_chain.push_back (cid);
  }
  delete_clause (c);
  return true;
}

void LidrupTracer::insert () {
  if (num_clauses == size_clauses)
    enlarge_clauses ();
  const uint64_t h = reduce_hash (compute_hash (last_id), size_clauses);
  LidrupClause *c = new_clause ();
  c->next = clauses[h];
  clauses[h] = c;
}

/*------------------------------------------------------------------------*/

inline void LidrupTracer::flush_if_piping () {
  if (piping)
    file->flush ();
}

inline void LidrupTracer::put_binary_zero () {
  assert (binary);
  assert (file);
  file->put ((unsigned char) 0);
}

inline void LidrupTracer::put_binary_lit (int lit) {
  assert (binary);
  assert (file);
  assert (lit != INT_MIN);
  unsigned x = 2 * abs (lit) + (lit < 0);
  unsigned char ch;
  while (x & ~0x7f) {
    ch = (x & 0x7f) | 0x80;
    file->put (ch);
    x >>= 7;
  }
  ch = x;
  file->put (ch);
}

inline void LidrupTracer::put_binary_id (int64_t id, bool can_be_negative) {
  assert (binary);
  assert (file);
  uint64_t x = abs (id);
  if (can_be_negative) {
    x = 2 * x + (id < 0);
  }
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

void LidrupTracer::lidrup_add_restored_clause (int64_t id) {
  if (!batch_weaken.empty () || !batch_delete.empty ())
    lidrup_batch_weaken_restore_and_delete ();
  batch_restore.push_back (id);
}

void LidrupTracer::add_original_clause_with_signature (int64_t id,
    const std::vector<int> & clause, const std::vector<uint8_t>& signature) {
  lidrup_batch_weaken_restore_and_delete ();
  if (internal->is_locally_produced_lrat_id (id)) {
    printf("ERROR: Invalid imported ID %lu for solver %i out of %i!\n", id,
      internal->opts.lratsolverid, internal->opts.lratsolvercount);
    abort();
  }
  // Also remember the imported ID so we can avoid to re-import it as long as it wasn't deleted.
  internal->active_imported_ids.insert(id);
  cb_import (id, clause.data (), clause.size (), signature.data (), signature.size ());
}

void LidrupTracer::lidrup_add_derived_clause (
    int64_t id, const vector<int> &clause, const vector<int64_t> &chain) {
  lidrup_batch_weaken_restore_and_delete ();
  if (callbacks) {
    if (!internal->is_locally_produced_lrat_id (id)) {
      printf("ERROR: Invalid ID %lu for solver %i out of %i!\n", id,
        internal->opts.lratsolverid, internal->opts.lratsolvercount);
      abort();
    }
    // We can cast from int64_t to uint64_t here because we only use RUP steps.
    cb_produce (id, clause.data (), clause.size (), (uint64_t*) chain.data (), chain.size (), clause.size ());
    return;
  }
  if (binary) {
    file->put ('l');
    put_binary_id (id);
  } else {
    file->put ("l ");
    file->put (id);
    file->put (' ');
  }
  for (const auto &external_lit : clause)
    if (binary)
      put_binary_lit (external_lit);
    else
      file->put (external_lit), file->put (' ');
  if (binary)
    put_binary_zero ();
  else
    file->put ("0 ");
  for (const auto &cid : chain)
    if (binary)
      put_binary_id (cid);
    else
      file->put (cid), file->put (' ');
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
}

void LidrupTracer::lidrup_add_original_clause (int64_t id,
                                               const vector<int> &clause) {
  lidrup_batch_weaken_restore_and_delete ();
  if (callbacks) return;
  if (binary) {
    file->put ('i');
    put_binary_id (id);
  } else {
    file->put ("i ");
    file->put (id);
    file->put (' ');
  }
  for (const auto &external_lit : clause)
    if (binary)
      put_binary_lit (external_lit);
    else
      file->put (external_lit), file->put (' ');
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
}

void LidrupTracer::lidrup_batch_weaken_restore_and_delete () {
  assert (batch_weaken.empty () || batch_delete.empty ());
  if (!batch_weaken.empty ()) {
    if (!callbacks) {
      if (binary) {
        file->put ('w');
      } else {
        file->put ("w ");
      }
      for (const auto &id : batch_weaken) {
        if (binary)
          put_binary_id (id);
        else
          file->put (id), file->put (' ');
      }
    }
    batch_weaken.clear ();
    if (!callbacks) {
      if (binary)
        put_binary_zero ();
      else
        file->put ("0\n");
    }
#ifndef QUIET
    batched++;
#endif
  }
  if (!batch_delete.empty ()) {
    if (callbacks) {
      // We can cast from int64_t to uint64_t here because we only use RUP steps.
      cb_delete ((uint64_t*) batch_delete.data (), batch_delete.size ());
      batch_delete.clear ();
      return;
    }
    if (binary) {
      file->put ('d');
    } else {
      file->put ("d ");
    }
    for (const auto &id : batch_delete) {
      if (binary)
        put_binary_id (id);
      else
        file->put (id), file->put (' ');
    }
    batch_delete.clear ();
    if (binary)
      put_binary_zero ();
    else
      file->put ("0\n");
#ifndef QUIET
    batched++;
#endif
  }
  if (!batch_restore.empty ()) {
    if (!callbacks) {
      if (binary) {
        file->put ('r');
      } else {
        file->put ("r ");
      }
      for (const auto &id : batch_restore) {
        if (binary)
          put_binary_id (id);
        else
          file->put (id), file->put (' ');
      }
    }
    batch_restore.clear ();
    if (!callbacks) {
      if (binary)
        put_binary_zero ();
      else
        file->put ("0\n");
    }
#ifndef QUIET
    batched++;
#endif
  }
}

void LidrupTracer::lidrup_conclude_and_delete (
    const vector<int64_t> &conclusion) {
  lidrup_batch_weaken_restore_and_delete ();
  uint64_t size = conclusion.size ();
  if (size > 1 && !callbacks) {
    if (binary) {
      file->put ('U');
      put_binary_id (size);
    } else {
      file->put ("U ");
      file->put (size), file->put ("\n");
    }
  }
  bool conclusionOk = !callbacks;
  if (callbacks) assert (size == 1);
  for (auto &id : conclusion) {
    if (!callbacks) {
      if (binary)
        file->put ('u');
      else
        file->put ("u ");
    }
    if (!find_and_delete (id)) {
      // id was *not* contained in lookup table
      assert (imported_clause.empty ());
      assert (conclusion.size () == 1); // must be the only one (?)
      if (callbacks) {
        cb_conclude (id);
        conclusionOk = true;
      } else if (binary) {
        put_binary_zero ();
        put_binary_id (id);
        put_binary_zero ();
      } else {
        file->put ("0 ");
        file->put (id);
        file->put (" 0\n");
      }
    } else {
      if (callbacks) {
        if (imported_chain.size () == 1) {
          // Only a single dependency: I think we can just replace the conclusion ID with it?
          cb_conclude (imported_chain.back ());
          conclusionOk = true;
        } else {
          // Several dependencies: Actually need to add as a new clause
          lidrup_add_derived_clause(id, imported_clause, imported_chain);
          // Delete at the beginning of the next call, see lidrup_solve_query
          helpers_to_delete.push_back (id);
          cb_conclude (id);
          conclusionOk = true;
        }
      } else {
        for (const auto &external_lit : imported_clause) {
          // flip sign...
          const auto not_elit = -external_lit;
          if (binary)
            put_binary_lit (not_elit);
          else
            file->put (not_elit), file->put (' ');
        }
        if (binary)
          put_binary_zero ();
        else
          file->put ("0 ");
        for (const auto &cid : imported_chain) {
          if (binary)
            put_binary_id (cid);
          else
            file->put (cid), file->put (' ');
        }
        if (binary)
          put_binary_zero ();
        else
          file->put ("0\n");
      }
      imported_clause.clear ();
      imported_chain.clear ();
    }
  }
  if (!conclusionOk) {
    printf("[WARN] CaDiCaL: Not reporting any conclusion ID -"
      " imported_clause len %lu, imported_chain len %lu conclusion len %lu\n",
      imported_clause.size (), imported_chain.size (), conclusion.size ());
    // abort ();
    // Let's roll with it – if the empty clause is fine as a conclusion clause,
    // our interface will auto-detect it even if we don't export a conclusion ID.
  }
  flush_if_piping ();
}

void LidrupTracer::lidrup_report_status (int status) {
  lidrup_batch_weaken_restore_and_delete ();
  if (callbacks) return;
  if (binary)
    file->put ('s');
  else
    file->put ("s ");
  if (status == SATISFIABLE)
    file->put ("SATISFIABLE");
  else if (status == UNSATISFIABLE)
    file->put ("UNSATISFIABLE");
  else
    file->put ("UNKNOWN");
  if (!binary)
    file->put ("\n");
  flush_if_piping ();
}

void LidrupTracer::lidrup_conclude_sat (const vector<int> &model) {
  lidrup_batch_weaken_restore_and_delete ();
  if (callbacks) return;
  if (binary)
    file->put ('m');
  else
    file->put ("m ");
  for (auto &lit : model) {
    if (binary)
      put_binary_lit (lit);
    else
      file->put (lit), file->put (' ');
  }
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
  flush_if_piping ();
}

void LidrupTracer::lidrup_conclude_unknown (const vector<int> &trail) {
  lidrup_batch_weaken_restore_and_delete ();
  if (callbacks) return;
  if (binary)
    file->put ('e');
  else
    file->put ("e ");
  for (auto &lit : trail) {
    if (binary)
      put_binary_lit (lit);
    else
      file->put (lit), file->put (' ');
  }
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
  flush_if_piping ();
}

void LidrupTracer::lidrup_solve_query () {
  for (auto id : helpers_to_delete) batch_delete.push_back (id);
  helpers_to_delete.clear ();
  lidrup_batch_weaken_restore_and_delete ();
  if (callbacks) return;
  if (binary)
    file->put ('q');
  else
    file->put ("q ");
  for (auto &lit : assumptions) {
    if (binary)
      put_binary_lit (lit);
    else
      file->put (lit), file->put (' ');
  }
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
  flush_if_piping ();
}

/*------------------------------------------------------------------------*/

void LidrupTracer::add_derived_clause (int64_t id, bool, int,
                                       const vector<int> &clause,
                                       const vector<int64_t> &chain) {
  if (!callbacks && file->closed ())
    return;
  assert (imported_clause.empty ());
  LOG (clause, "LIDRUP TRACER tracing addition of derived clause");
  lidrup_add_derived_clause (id, clause, chain);
#ifndef QUIET
  added++;
#endif
}

void LidrupTracer::add_assumption_clause (int64_t id,
                                          const vector<int> &clause,
                                          const vector<int64_t> &chain) {
  if (!callbacks && file->closed ())
    return;
  assert (imported_clause.empty ());
  LOG (clause,
       "LIDRUP TRACER tracing addition of assumption clause[%" PRId64 "]",
       id);
  for (auto &lit : clause)
    imported_clause.push_back (lit);
  for (auto &cid : chain)
    imported_chain.push_back (cid);
  last_id = id;
  insert ();
  imported_clause.clear ();
  imported_chain.clear ();
}

void LidrupTracer::delete_clause (int64_t id, bool, const vector<int> &) {
  if (!callbacks && file->closed ())
    return;
  assert (imported_clause.empty ());
  LOG ("LIDRUP TRACER tracing deletion of clause[%" PRId64 "]", id);
  if (!internal->is_locally_produced_lrat_id(id))
    internal->active_imported_ids.erase(id);
  if (find_and_delete (id)) {
    assert (imported_clause.empty ());
    if (!batch_delete.empty () || !batch_restore.empty ())
      lidrup_batch_weaken_restore_and_delete ();
    batch_weaken.push_back (id);
#ifndef QUIET
    weakened++;
#endif
  } else {
    if (!batch_weaken.empty () || !batch_restore.empty ())
      lidrup_batch_weaken_restore_and_delete ();
    batch_delete.push_back (id);
#ifndef QUIET
    deleted++;
#endif
  }
}

void LidrupTracer::weaken_minus (int64_t id, const vector<int> &) {
  if (!callbacks && file->closed ())
    return;
  assert (imported_clause.empty ());
  LOG ("LIDRUP TRACER tracing weaken minus of clause[%" PRId64 "]", id);
  last_id = id;
  insert ();
}

void LidrupTracer::conclude_unsat (ConclusionType con,
                                   const vector<int64_t> &conclusion) {
  if (!callbacks && file->closed ())
    return;
  assert (imported_clause.empty ());
  if (callbacks) assert (con == CONFLICT || con == ASSUMPTIONS);
  if (con == CONFLICT) assert (conclusion.size () == 1);
  LOG (conclusion, "LIDRUP TRACER tracing conclusion of clause(s)");
  lidrup_conclude_and_delete (conclusion);
}

void LidrupTracer::add_original_clause (int64_t id, bool,
                                        const vector<int> &clause,
                                        bool restored) {
  if (!callbacks && file->closed ())
    return;
  if (!restored) {
    LOG (clause, "LIDRUP TRACER tracing addition of original clause");
#ifndef QUIET
    original++;
#endif
    return lidrup_add_original_clause (id, clause);
  }
  assert (restored);
  if (find_and_delete (id)) {
    LOG (clause,
         "LIDRUP TRACER the clause was not yet weakened, so no restore");
    return;
  }
  LOG (clause, "LIDRUP TRACER tracing addition of restored clause");
  lidrup_add_restored_clause (id);
#ifndef QUIET
  restore++;
#endif
}

void LidrupTracer::report_status (int status, int64_t) {
  if (!callbacks && file->closed ())
    return;
  LOG ("LIDRUP TRACER tracing report of status %d", status);
  lidrup_report_status (status);
}

void LidrupTracer::conclude_sat (const vector<int> &model) {
  if (!callbacks && file->closed ())
    return;
  LOG (model, "LIDRUP TRACER tracing conclusion of model");
  lidrup_conclude_sat (model);
}

void LidrupTracer::conclude_unknown (const vector<int> &entrailed) {
  if (!callbacks && file->closed ())
    return;
  LOG (entrailed, "LIDRUP TRACER tracing conclusion of UNK");
  lidrup_conclude_unknown (entrailed);
}

void LidrupTracer::solve_query () {
  if (!callbacks && file->closed ())
    return;
  LOG (assumptions, "LIDRUP TRACER tracing solve query with assumptions");
  lidrup_solve_query ();
#ifndef QUIET
  solved++;
#endif
}

void LidrupTracer::add_assumption (int lit) {
  LOG ("LIDRUP TRACER tracing addition of assumption %d", lit);
  assumptions.push_back (lit);
}

void LidrupTracer::reset_assumptions () {
  LOG (assumptions, "LIDRUP TRACER tracing reset of assumptions");
  assumptions.clear ();
}

/*------------------------------------------------------------------------*/

bool LidrupTracer::closed () { return file->closed (); }

#ifndef QUIET

void LidrupTracer::print_statistics () {
  // TODO complete this.
  uint64_t bytes = file->bytes ();
  uint64_t total = added + deleted + weakened + restore + original;
  MSG ("LIDRUP %" PRId64 " original clauses %.2f%%", original,
       percent (original, total));
  MSG ("LIDRUP %" PRId64 " learned clauses %.2f%%", added,
       percent (added, total));
  MSG ("LIDRUP %" PRId64 " deleted clauses %.2f%%", deleted,
       percent (deleted, total));
  MSG ("LIDRUP %" PRId64 " weakened clauses %.2f%%", weakened,
       percent (weakened, total));
  MSG ("LIDRUP %" PRId64 " restored clauses %.2f%%", restore,
       percent (restore, total));
  MSG ("LIDRUP %" PRId64 " batches of deletions, weaken and restores %.2f",
       batched, relative (batched, deleted + restore + weakened));
  MSG ("LIDRUP %" PRId64 " queries %.2f", solved, relative (solved, total));
  MSG ("LIDRUP %" PRId64 " bytes (%.2f MB)", bytes,
       bytes / (double) (1 << 20));
}

#endif

void LidrupTracer::close (bool print) {
  assert (!closed ());
  file->close ();
#ifndef QUIET
  if (print) {
    MSG ("LIDRUP proof file '%s' closed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

void LidrupTracer::flush (bool print) {
  assert (!closed ());
  lidrup_batch_weaken_restore_and_delete ();
  file->flush ();
#ifndef QUIET
  if (print) {
    MSG ("LIDRUP proof file '%s' flushed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

} // namespace CaDiCaL
