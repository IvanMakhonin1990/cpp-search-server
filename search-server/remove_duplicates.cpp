#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer &search_server) {
  set<size_t> docs_hash;
  vector<int> docs_to_remove;
  for (const int &document_id : search_server) {
    const auto words = search_server.GetWordFrequencies(document_id);
    size_t doc_hash = hash<size_t>{}(words.size());
    for (const auto &w : search_server.GetWordFrequencies(document_id)) {
      hash_combine(doc_hash, w.first);
    }
    if (docs_hash.count(doc_hash) != 0) {
      docs_to_remove.push_back(document_id);
    }
    docs_hash.insert(doc_hash);
  }
  for (const int &document_id : docs_to_remove) {
    cout << "Found duplicate document id " << document_id << endl;
    search_server.RemoveDocument(document_id);
  }
}