#pragma once

#include "search_server.h"
#include "document.h"

#include <string_view>
#include <set>

void PrintDocument(const Document &document);

void PrintMatchDocumentResult(int document_id,
                              const std::vector<std::string_view> &words,
                              DocumentStatus status);

std::vector<std::string_view> SplitIntoWords(std::string_view text);



template <typename StringContainer>
std::set<std::string, std::less<> > MakeUniqueNonEmptyStrings(const StringContainer &strings) {
  std::set<std::string, std::less<>> non_empty_strings;
  for (const auto &str : strings) {
    if (!str.empty()) {
      non_empty_strings.emplace(str);
    }
  }
  return non_empty_strings;
}
