#pragma once

#include <set>
#include <vector>
#include <algorithm>
#include <iostream>

#include "search_server.h"

template <class T> inline void hash_combine(std::size_t &seed, const T &v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

void RemoveDuplicates(SearchServer &search_server);