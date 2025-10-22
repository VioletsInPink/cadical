#include "sharingtracer.hpp"
#include "internal.hpp"
#include <cstdio>
#include <vector>

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

SharingTracer::SharingTracer(Internal *i, LratCallbackProduceClause cb_produce)
    : internal (i), cb_produce(cb_produce)
{
  (void) internal;
}

void SharingTracer::connect_internal (Internal *i) {
  internal = i;
}

SharingTracer::~SharingTracer () {}

/*------------------------------------------------------------------------*/

void SharingTracer::add_derived_clause (uint64_t id, bool,
                                       const vector<int> &clause,
                                       const vector<uint64_t> &chain) {
  cb_produce (id, clause.data (), clause.size (), chain.data (), chain.size (), clause.size ());
}

} // namespace CaDiCaL
