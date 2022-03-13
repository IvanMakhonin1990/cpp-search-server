#pragma once
#include <iostream>

struct Document {
  int id = 0;
  double relevance = 0.0;
  int rating = 0;

  Document() = default;

  Document(int id, double relevance, int rating);
};

std::ostream &operator<<(std::ostream &out, const Document &document);
