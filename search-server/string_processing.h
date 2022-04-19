#pragma once

#include "search_server.h"
#include "document.h"

#include <string_view>
#include <set>

void PrintDocument(const Document &document);

void PrintMatchDocumentResult(int document_id,
                              const std::vector<std::string_view> &words,
                              DocumentStatus status);

std::vector<std::string_view> SplitIntoWords(const std::string_view &text);



template <typename StringContainer>
std::set<std::string, std::less<> > MakeUniqueNonEmptyStrings(const StringContainer &strings) {
  std::set<std::string, std::less<>> non_empty_strings;
  for (const auto &str : strings) {
    if (!str.empty()) {
      non_empty_strings.insert(std::string(std::move_iterator(str.begin()),
                                           std::move_iterator(str.end())));
    }
  }
  return non_empty_strings;
}
