#include "string_processing.h"

#include <iostream>

using namespace std;

void PrintDocument(const Document &document) {
  cout << "{ "s
       << "document_id = "s << document.id << ", "s
       << "relevance = "s << document.relevance << ", "s
       << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string> &words,
                              DocumentStatus status) {
  cout << "{ "s
       << "document_id = "s << document_id << ", "s
       << "status = "s << static_cast<int>(status) << ", "s
       << "words ="s;
  for (const string &word : words) {
    cout << ' ' << word;
  }
  cout << "}"s << endl;
}

vector<string> SplitIntoWords(const string &text) {
  vector<string> words;
  string word;
  for (const char c : text) {
    if (c == ' ') {
      if (!word.empty()) {
        if (word.empty()) {
          continue;
        }
        words.push_back(word);
        word.clear();
      }
    } else {
      word += c;
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }

  return words;
}
