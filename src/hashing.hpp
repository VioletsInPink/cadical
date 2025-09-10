
#pragma once

#include <cstddef>
#include <functional>

template <class T>
inline void hash_combine(std::size_t & s, const T & v) {
    static std::hash<T> h;
    s ^= h(v) + 0x9e3779b9 + (s<< 6) + (s>> 2);
}

struct U64Hasher {
    size_t operator()(const size_t& input) const {
        size_t h = 17;
        hash_combine(h, input);
        return h;
    }
};
