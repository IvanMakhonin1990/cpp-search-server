#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <execution>
#include <unordered_set>
#include <string_view>
#include <cassert>

#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
  template <typename StringContainer>
  explicit SearchServer(const StringContainer &stop_words);

  explicit SearchServer(const std::string_view &stop_words_text);

  explicit SearchServer(const std::string &stop_words_text);

  void AddDocument(int document_id, const std::string_view &document,
                   DocumentStatus status, const std::vector<int> &ratings);

  template <typename DocumentPredicate>
  std::vector<Document>
  FindTopDocuments(std::string_view raw_query,
                   DocumentPredicate document_predicate) const;

  template <typename DocumentPredicate>
  std::vector<Document>
  FindTopDocuments(std::execution::parallel_policy polict, std::string_view raw_query,
                   DocumentPredicate document_predicate) const;

   template <typename DocumentPredicate>
  std::vector<Document>
  FindTopDocuments(std::execution::sequenced_policy polict,
                   std::string_view raw_query,
                   DocumentPredicate document_predicate) const;

  std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                         DocumentStatus status) const;

  std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, 
       std::string_view raw_query, DocumentStatus status) const;

  std::vector<Document> FindTopDocuments(std::execution::parallel_policy,
                                         std::string_view raw_query,
                                         DocumentStatus status) const;

  std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

  std::vector<Document> FindTopDocuments(std::execution::sequenced_policy,
       std::string_view raw_query) const;

  std::vector<Document> FindTopDocuments(std::execution::parallel_policy,
                                          std::string_view raw_query) const;

  int GetDocumentCount() const;

  std::set<int>::const_iterator begin() const;
  std::set<int>::const_iterator end() const;

  const std::map<std::string_view, double>&  GetWordFrequencies(int document_id) const;

  
  void RemoveDocument(std::execution::parallel_policy policy, int document_id);

  void RemoveDocument(std::execution::sequenced_policy policy, int document_id);

  void RemoveDocument(int document_id);
  
  std::tuple<std::vector<std::string_view>, DocumentStatus>
  MatchDocument(std::string_view raw_query, int document_id) const;

  std::tuple<std::vector<std::string_view>, DocumentStatus>
  MatchDocument(std::execution::sequenced_policy policy, std::string_view raw_query, int document_id) const;

  std::tuple<std::vector<std::string_view>, DocumentStatus>
  MatchDocument(std::execution::parallel_policy policy, std::string_view raw_query,
                int document_id) const;

private:
  static constexpr double DOUBLE_TOLERANCE = 1.0e-6;
  struct DocumentData {
    int rating;
    DocumentStatus status;
    const std::string document;
  };
  const std::set<std::string, std::less<>> stop_words_;
  std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
  std::map<int, std::set<std::string_view>> doc_to_words_freqs_;
  std::map<int, DocumentData> documents_;
  std::set<int> document_ids_;
  static std::map<std::string_view, double> result;
  std::string last_raw_query;

  struct QueryWord {
    std::string_view data;
    bool is_minus;
    bool is_stop;
  };
  
  struct Query {
    std::vector<std::string_view> plus_words;
    std::vector<std::string_view> minus_words;
  };

  bool IsStopWord(const std::string_view &word) const;
  static bool IsValidWord(const std::string_view &word);
  std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view &text) const;
  static int ComputeAverageRating(const std::vector<int> &ratings);
  QueryWord ParseQueryWord(const std::string_view &text) const;
  Query ParseQuery(const std::string_view &text, bool skip_sort=true) const;
  Query ParseQuery(std::execution::parallel_policy policy, const std::string_view &text, bool skip_sort = true) const;
  double ComputeWordInverseDocumentFreq(const std::string_view &word) const;
  template <typename DocumentPredicate> std::vector<Document> FindAllDocuments(const SearchServer::Query &query, DocumentPredicate document_predicate) const;
  template <typename DocumentPredicate> std::vector<Document> 
      FindAllDocuments(std::execution::parallel_policy, const SearchServer::Query &query, DocumentPredicate document_predicate) const;
  template <typename DocumentPredicate> std::vector<Document>
  FindAllDocuments(std::execution::sequenced_policy, const SearchServer::Query &query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words)
    : stop_words_(
          MakeUniqueNonEmptyStrings(stop_words)) // Extract non-empty stop words
{
  if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
    throw std::invalid_argument("Some of stop words are invalid");
  }
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::string_view raw_query,
                 DocumentPredicate document_predicate) const {
  const auto query = ParseQuery(raw_query, false);

  auto matched_documents = FindAllDocuments(query, document_predicate);

  sort(matched_documents.begin(), matched_documents.end(),
       [](const Document &lhs, const Document &rhs) {
         if (std::abs(lhs.relevance - rhs.relevance) < DOUBLE_TOLERANCE) {
           return lhs.rating > rhs.rating;
         } else {
           return lhs.relevance > rhs.relevance;
         }
       });
  if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
    matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
  }

  return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query,
                               DocumentPredicate document_predicate) const {
  const auto query = ParseQuery(policy, raw_query, false);

  auto matched_documents = FindAllDocuments(policy, query, document_predicate);

  sort(matched_documents.begin(), matched_documents.end(),
       [](const Document &lhs, const Document &rhs) {
         if (std::abs(lhs.relevance - rhs.relevance) < DOUBLE_TOLERANCE) {
           return lhs.rating > rhs.rating;
         } else {
           return lhs.relevance > rhs.relevance;
         }
       });
  if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
    matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
  }

  return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::execution::sequenced_policy policy,
                               std::string_view raw_query,
                               DocumentPredicate document_predicate) const {
  return FindTopDocuments(raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(const SearchServer::Query &query,
                 DocumentPredicate document_predicate) const {
  std::map<int, double> document_to_relevance;
  for (const auto &word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
    for (const auto [document_id, term_freq] :
         word_to_document_freqs_.at(word)) {
      const auto &document_data = documents_.at(document_id);
      if (document_predicate(document_id, document_data.status,
                             document_data.rating)) {
        document_to_relevance[document_id] += term_freq * inverse_document_freq;
      }
    }
  }
  
  for (const auto &word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
      document_to_relevance.erase(document_id);
    }
  }

  std::vector<Document> matched_documents;
  for (const auto [document_id, relevance] : document_to_relevance) {
    matched_documents.push_back(
        {document_id, relevance, documents_.at(document_id).rating});
  }
  return matched_documents;
}
template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(std::execution::sequenced_policy policy,
                               const SearchServer::Query &query,
                               DocumentPredicate document_predicate) const {
  return FindAllDocuments(query, document_predicate);
}

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, Predicate predicate) const
{
    ConcurrentMap<int, double> document_to_relevance(100);
      std::set<std::string_view, std::less<>> minus_words(query.minus_words.begin(),
                                                     query.minus_words.end());
      auto is_minus_word = [&](std::string_view word){
        return minus_words.end() != minus_words.find(word);
      };

    for_each(std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [&](std::string_view word)
        {
           
            if (word_to_document_freqs_.count(word) && !is_minus_word(word)) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                std::for_each(/*std::execution::par,*/
                    word_to_document_freqs_.at(std::string(word)).begin(), word_to_document_freqs_.at(std::string(word)).end(),
                    [&](const auto& doc_freq)
                    {
                        const auto& doc_data = documents_.at(doc_freq.first);
                        if (predicate(doc_freq.first, doc_data.status, doc_data.rating)) {
                            document_to_relevance[doc_freq.first].ref_to_value += doc_freq.second * inverse_document_freq;
                        }
                    });
            }
        });

    std::atomic_int size = 0;
    const std::map<int, double>& ord_map = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents(ord_map.size());

    std::for_each(std::execution::par,
        ord_map.begin(), ord_map.end(),
        [&](const auto& map)
        {
            matched_documents[size++] = { map.first, map.second, documents_.at(map.first).rating };
        });

    matched_documents.resize(size);

    return matched_documents;
}

std::vector<std::vector<Document>>
ProcessQueries(const SearchServer &search_server,
               const std::vector<std::string> &queries);
