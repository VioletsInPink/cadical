
#include "internal.hpp"
#include <cstdint>
#include <sstream>
#include <string>
#include <iomanip>
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)

// Write a LRAT derivation straight to the tracers
// without the detour over "Proof::add_derived_*_clause".
void CaDiCaL::Internal::add_clause_to_proof (uint64_t id) {
  assert (is_locally_produced_lrat_id (id));

  // externalize literals
  std::vector<int> elits;
  for (int ilit : clause) elits.push_back(externalize (ilit));

  // debug logging
  if (opts.lratdebug) {
    if (!dbg_ofs_import_simplifications.is_open ()) {
      dbg_ofs_import_simplifications = std::ofstream(".importsimpl." + std::to_string(opts.lratsolverid) + "." + std::to_string(gettid()));
    }
    std::string clause_str;
    for (int elit : elits) clause_str += std::to_string(elit) + " ";
    std::string chain_str;
    for (auto chain_id : lrat_chain) chain_str += std::to_string(chain_id) + " ";
    dbg_ofs_import_simplifications << id << " : " << clause_str << " - " << chain_str << std::endl;
    dbg_ofs_import_simplifications.flush();
  }

  // add to proofs / tracers
  for (auto &tracer : proof->get_tracers ()) {
    tracer->add_derived_clause (id, /*redundant=*/true, elits, lrat_chain);
  }
}

// Adjusted and simplified version of Internal::search_assign (analyze.cpp).
// We do not add anything to the proof and we do not re-export the unit
// (both is done in try_import_unit, but only if a simplification was done).
void CaDiCaL::Internal::learn_imported_unit_clause (uint64_t id, int lit) {

  const int idx = vidx (lit);
  assert (!val (idx));
  Var &v = var (idx);
  const int lit_level = 0; // unit

  v.level = lit_level;
  v.trail = trail.size ();
  v.reason = 0;
  assert ((int) num_assigned < max_var);
  assert (num_assigned == trail.size ());
  num_assigned++;

  assert (!unsat);
  const unsigned uidx = vlit (lit);
  if (lrat || frat) unit_clauses (uidx) = id;
  register_lrat_id_of_unit_ilit (id, lit);
  mark_fixed (lit);

  const signed char tmp = sign (lit);
  set_val (idx, tmp);
  assert (val (lit) > 0);  // Just a bit paranoid but useful.
  assert (val (-lit) < 0); // Ditto.
  if (!searching_lucky_phases)
    phases.saved[idx] = tmp; // phase saving during search
  trail.push_back (lit);
#ifdef LOGGING
  if (!lit_level)
    LOG ("root-level unit assign %d @ 0", lit);
  else
    LOG (reason, "search assign %d @ %d", lit, lit_level);
#endif

  if (watching ()) {
    const Watches &ws = watches (-lit);
    if (!ws.empty ()) {
      const Watch &w = ws[0];
      __builtin_prefetch (&w, 0, 1);
    }
  }

  internal->stats.clauseimport.imported++;
}

// Attempt to import an incoming unit clause, possibly arising from
// a simplification of an incoming non-unit clause (bool simplified).
// In that case, lrat_chain may contain IDs of this simplification.
void CaDiCaL::Internal::try_import_unit (uint64_t id, int elit, bool simplified, const std::vector<uint8_t>& sig) {

  assert (clause.empty ());
  assert (!lrat || !simplified == lrat_chain.empty ());

  // Do not learn unit clause if marked as witness
  if (external->marked (external->witness, elit)) {
    internal->stats.clauseimport.r_wit++;
    internal->stats.clauseimport.discarded++;
    if (simplified && lrat) lrat_chain.clear ();
    return;
  }
  int ilit = external->internalize (elit);
  auto& f = flags(ilit);
  // Do not import eliminated or substituted literal
  if (f.eliminated () || f.substituted ()) {
    internal->stats.clauseimport.r_el++;
    internal->stats.clauseimport.discarded++;
    if (simplified && lrat) lrat_chain.clear ();
    return;
  }
  // Do not import units which are already fixed
  if (f.fixed () || f.pure ()) {
    internal->stats.clauseimport.r_fx++;
    internal->stats.clauseimport.discarded++;
    if (simplified && lrat) lrat_chain.clear ();
    return;
  }
  // Actually add the unit clause
  uint64_t impclsid = simplified ? next_lrat_id () : id;
  if (simplified) {
    // Clause was simplified: Need to add an LRAT derivation
    if (lrat) {
      assert(!lrat_chain.empty ());
      lrat_chain.push_back (id); // add ID of the "original" incoming clause!
      clause.resize (1, ilit);
      add_clause_to_proof (impclsid);
      lrat_chain.clear ();
      clause.clear ();
    }
    // Re-export the clause in its simplified form
    if (!opts.signsharedcls)
      external->export_learned_internal_unit_clause (impclsid, ilit);
  } else if (opts.signsharedcls) {
    // Clause was not simplified but originally a unit: add to proof
    std::vector<int> cls(1, elit);
    validate_clause_and_add_as_axiom(id, cls, sig);
  }
  learn_imported_unit_clause (impclsid, ilit);
}

void CaDiCaL::Internal::validate_clause_and_add_as_axiom (uint64_t id, std::vector<int>& cls, const std::vector<uint8_t>& sig) {

  // Validate the clause's signature
  if (!opts.signsharedcls) return;
  // Forward the clause to the checker/proof **as an axiom** (i.e., "original" clause)
  // and to validate the signature.
  for (auto tracer : proof->get_tracers ()) {
    tracer->add_original_clause_with_signature (id, cls, sig);
  }
  stats.validated_incoming_cls++;
}

// Attempt to import a single clause with external literals.
void CaDiCaL::Internal::handle_incoming_clause (uint64_t id, int glue, std::vector<int>& cls, const std::vector<uint8_t>& sig) {

  // If no LRAT ID was provided, assign a new one internally.
  // (We're not doing proper proof logging in that case either way,
  // but non-zero IDs are still important for some data structures.)
  if (id == 0) {
    assert (!opts.lrat);
    id = next_lrat_id ();
  }

  const size_t size = cls.size ();
  assert (size > 0);
  assert (clause.empty ());
  if (lrat) {
    assert (lrat_chain.empty ());
    assert (opts.signsharedcls || !is_locally_produced_lrat_id (id));
    if (is_locally_produced_lrat_id (id)) return; // no need to re-add your own clause
  }

  // Unit clause?
  if (size == 1) {
    try_import_unit (id, cls[0], false, sig);
    clause.clear ();
    return;
  }

  // Handle non-unit clause
  assert (glue > 0);
  bool reducedSize = false;

  // Analyze clause literals
  bool addClause = true;
  for (size_t i = 0; i < size; i++) {

    int elit = cls[i];
    assert (elit != 0);

    if (external->marked (external->witness, elit)) {
      // Literal marked as witness: Cannot import
      internal->stats.clauseimport.r_wit++;
      addClause = false; break;
    }

    int ilit = external->internalize(elit);

    auto& f = flags (ilit);
    if (f.eliminated () || f.substituted ()) {
      // Literal has been eliminated: do not add this clause.
      internal->stats.clauseimport.r_el++;
      addClause = false; break;
    }
    if (f.pure ()) {
      assert (val (ilit) != 0);
      addClause = false; break;
    }
    if (f.fixed ()) {
      // Literal is fixed
      if (val (ilit) == 1) {
        // TRUE: Clause must be omitted.
        internal->stats.clauseimport.r_fx++;
        addClause = false; break;
      } // else: FALSE - literal must be omitted from the clause.
      assert (val (ilit) == -1);
      reducedSize = true;
      if (lrat) {
        // Add the unit clause causing the shortening to the LRAT chain.
        // We look up the *external* literal so that compacting does not
        // destroy the mapping.
        auto& unit_ids = external->ext_units;
        unsigned eidx = (-elit > 0) + 2u * (unsigned) abs (-elit);
        assert (eidx < unit_ids.size ());
        uint64_t uid = unit_ids[eidx];
        assert (uid);
        lrat_chain.push_back (uid);
      }
    } else {
      // Can treat literal normally.
      clause.push_back (ilit);
    }
  }

  // Can clause be imported?
  if (!addClause) {
    internal->stats.clauseimport.discarded++;
    clause.clear ();
    lrat_chain.clear ();
    return;
  }

  validate_clause_and_add_as_axiom (id, cls, sig);

  // Clause can be imported. Which size?
  if (clause.empty ()) {
    // found empty clause -- UNSAT
    assert (reducedSize);
    if (lrat) {
      assert (!lrat_chain.empty ());
      lrat_chain.push_back (id); // add ID of the "original" incoming clause!
      add_clause_to_proof (next_lrat_id ());
      lrat_chain.clear ();
    }
    internal->stats.clauseimport.r_fx++;
    internal->stats.clauseimport.discarded++;
    unsat = true;
    return;
  }
  if (clause.size () == 1) {
    // unit clause due to shortening -- to be handled below
    assert (reducedSize);
    int elit = internal->externalize (clause[0]);
    clause.clear ();
    try_import_unit (id, elit, true, sig);
    clause.clear ();
    return;
  }
  // Handle clause of size >= 2 being learnt
  uint64_t impclsid = reducedSize ? next_lrat_id () : id;
  if (reducedSize && lrat) {
    assert (!lrat_chain.empty ());
    lrat_chain.push_back (id); // add ID of the "original" incoming clause!
    add_clause_to_proof (impclsid);
    lrat_chain.clear ();
  }
  Clause * res = new_clause (true, glue, reducedSize, impclsid);
  clause.clear ();
  assert (watching ());
  watch_clause (res);
  internal->stats.clauseimport.imported++;
}


bool CaDiCaL::Internal::importing () {
  return level == 0 && external->learnSource != 0 
      && watching() && external->learnSource->hasNextClause ();
}

// Top-level import function
void CaDiCaL::Internal::import_redundant_clauses (int& res) {
  if (external->learnSource == 0) return;
  if (res != 0) return;
  std::vector<uint8_t>* sig;
  std::vector<uint8_t> emptySig;

  // Import external clauses.
  while (external->learnSource->hasNextClause ()) {

    // Fetch pointer to 1st literal and size of the clause (plus ID, glue)
    uint64_t id;
    int glue;
    auto cls = external->learnSource->getNextClause (id, glue, sig);
    handle_incoming_clause (id, glue, cls, sig ? *sig : emptySig);
    stats.incoming_cls++;

    // Stop importing if SAT or UNSAT was found
    if (unsat || res == 20) {
      res = 20;
      return;
    }
    if (satisfied ()) {
      res = 10;
      return;
    }
  }
}
