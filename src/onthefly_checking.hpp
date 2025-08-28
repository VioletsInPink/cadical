
#pragma once

#include <cstdint>
#include <functional>

/*
Check and add a clause derivation. If a signature pointer is provided, compute
a signature for the clause and store it at the pointed-to place.
Arguments:
    - unsigned long : LRAT clause ID
    - const int* : literal data
    - int : number of literals
    - const unsigned long* : clause hints
    - int : number of clause hints
    - int : glue of the learnt clause, or 0 if it is irrelevant and the clause should NOT be exported
*/
typedef std::function<void(unsigned long, const int*, int, const unsigned long*, int, int)> LratCallbackProduceClause;

/*
Add a clause as an axiom (i.e., as if it were an original clause of the problem)
while validating the provided signature.
Arguments:
    - unsigned long : LRAT clause ID
    - const int* : literal data
    - int : number of literals
    - const uint8_t* : clause signature to validate
    - int : size of the signature (in bytes)
*/
typedef std::function<void(unsigned long, const int*, int, const uint8_t*, int)> LratCallbackImportClause;

/*
Delete a number of clauses, specified via LRAT IDs.
Arguments:
    - const unsigned long* : clause IDs
    - int : number of clause IDs
*/
typedef std::function<void(const unsigned long*, int)> LratCallbackDeleteClauses;

typedef std::function<void(unsigned long)> LratCallbackConcludeUnsat;
