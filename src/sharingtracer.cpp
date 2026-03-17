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

void SharingTracer::add_derived_clause (int64_t id, bool, int,
                                       const vector<int> &clause,
                                       const vector<int64_t> &chain) {
  if (internal->transformed_id) {
    id = internal->transformed_id;
    internal->transformed_id = 0;
  }
  cb_produce (id, clause.data (), clause.size (), (uint64_t*) chain.data (), chain.size (), clause.size ());
}

} // namespace CaDiCaL
