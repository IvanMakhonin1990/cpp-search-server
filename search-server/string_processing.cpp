#include "string_processing.h"

#include <iostream>

using namespace std;

void PrintDocument(const Document &document) {
  cout << "{ "s
       << "document_id = "s << document.id << ", "s
       << "relevance = "s << document.relevance << ", "s
       << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string_view> &words,
                              DocumentStatus status) {
  cout << "{ "s
       << "document_id = "s << document_id << ", "s
       << "status = "s << static_cast<int>(status) << ", "s
       << "words ="s;
  for (const auto &word : words) {
    cout << ' ' << word;
  }
  cout << "}"s << endl;
}

vector<string_view> SplitIntoWords(string_view text) {
  vector<string_view> words;

  auto begin = text.begin();
  auto it = text.begin();
  size_t count = 0;
  while (text.end() != it) {
    if (*it == ' ') {
      if (count > 0) {
        //(it + 1 == text.end()) ? (it + 1) : it
        // = {begin, begin + count};
        string_view word = text.substr(distance(text.begin(), begin), count);
        // if (!word.empty()) {
        words.push_back(word);
        //}
      }
      count = 0;
      begin = it + 1;

    } else {
      ++count;
      if (it + 1 == text.end()) {
        words.push_back(text.substr(distance(text.begin(), begin), count));
      }
    }
    ++it;
  }
  return words;
}
