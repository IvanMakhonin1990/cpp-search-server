﻿#include "search_server.h"
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <execution>
#include <deque>


#include "log_duration.h"

using namespace std;

SearchServer::SearchServer(const string_view &stop_words_text)   
    : SearchServer(SplitIntoWords(stop_words_text)) {}

SearchServer::SearchServer(const string &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const string_view &document,
                               DocumentStatus status,
                               const vector<int> &ratings) {
  if ((document_id < 0) || (documents_.count(document_id) > 0)) {
    throw invalid_argument("Invalid document_id"s);
  }

  const auto [it, inserted] = documents_.emplace(
      document_id, DocumentData{ComputeAverageRating(ratings), status, string(document)});
  const auto words = SplitIntoWordsNoStop(it->second.document);

  const double inv_word_count = 1.0 / words.size();
  for (const auto &word : words) {
    word_to_document_freqs_[word][document_id] += inv_word_count;
    doc_to_words_freqs_[document_id].emplace(word);
  }
  document_ids_.insert(document_id);
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(string_view raw_query, int document_id) const {
  const auto query = ParseQuery(raw_query, false);

  vector<string_view> matched_words;
  auto f = [&](const string_view &word) {
    return ((word_to_document_freqs_.count(word) != 0) &&
            (word_to_document_freqs_.at(word).count(document_id) > 0));
  };
  auto it = query.minus_words.empty() ? query.minus_words.end()
                                      : find_if(query.minus_words.begin(),
                                                query.minus_words.end(), f);
  if (query.minus_words.end() == it) {

    for (const auto &word : query.plus_words) {
      auto item = word_to_document_freqs_.find(word);
      if (word_to_document_freqs_.end()==item) {
        continue;
      }
      
      if (item->second.count(document_id) > 0) {
        matched_words.push_back(item->first);
      }
    }

  }
  return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(execution::sequenced_policy policy,
                            string_view raw_query, int document_id) const {
  return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(execution::parallel_policy policy,
                            string_view raw_query, int document_id) const {
  auto words = SplitIntoWords(raw_query);

  vector<string_view> matched_words;

  vector<string_view> tmp_words(words.size());

  auto f = [&](const string_view &word) {
    auto it = word_to_document_freqs_.find(string(word));
    return pair(((word_to_document_freqs_.end() != it) &&
                 (it->second.count(document_id) > 0)),
                it);
  };
  bool is_valid = true;
  bool found = false;
  transform(policy, words.begin(), words.end(), tmp_words.begin(),
            [&](auto &word) {
              if (is_valid) {
                auto query_word = ParseQueryWord(word);
                auto item = f(query_word.data);
                if (!query_word.is_stop && item.first) {
                  if (query_word.is_minus) {
                    is_valid = false;
                  } else {
                    found = true;
                    return item.second->first;
                  }
                }
              }
              return string_view();
            });
  if (is_valid && found) {
    sort(policy, tmp_words.begin(), tmp_words.end());
    auto u_it = unique(policy, tmp_words.begin(), tmp_words.end());
    auto it = move_iterator(tmp_words.begin());
    auto size = distance(tmp_words.begin(), u_it);
    if (tmp_words[0].empty()) {
      ++it;
      --size;
    }
    matched_words = vector<string_view>(size);
    copy(policy, it, move_iterator(u_it), matched_words.begin());
  }
  return {matched_words, documents_.at(document_id).status};
  /*const auto query = ParseQuery(raw_query);

  const auto status = documents_.at(document_id).status;

  auto f = [&](const string_view &word) {
    auto it = word_to_document_freqs_.find(string(word));
    return (word_to_document_freqs_.end() != it) &&
           (it->second.count(document_id) > 0);
  };

  if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(),
             f)) {
    return {{}, status};
  }
  vector<string_view> matched_words(query.plus_words.size());
  auto words_end = copy_if(execution::par, query.plus_words.begin(),
                           query.plus_words.end(), matched_words.begin(), f);
  sort(matched_words.begin(), words_end);
  words_end = unique(matched_words.begin(), words_end);
  matched_words.erase(words_end, matched_words.end());

  return {matched_words, documents_.at(document_id).status};*/
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query,
                                                DocumentStatus status) const {
  return FindTopDocuments(
      raw_query, [status](int document_id, DocumentStatus document_status,
                          int rating) { return document_status == status; });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

bool SearchServer::IsStopWord(const string_view &word) const {
  return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view &word) {
  // A valid word must not contain special characters
  return none_of(word.begin(), word.end(),
                 [](char c) { return c >= '\0' && c < ' '; });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view &text) const {
  vector<string_view> words;
  for (const auto &word : SplitIntoWords(text)) {
    if (!IsValidWord(word)) {
      throw invalid_argument("Word "s + string(word) + " is invalid"s);
    }
    if (!IsStopWord(word)) {
      words.push_back(word);
    }
  }
  return words;
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
  if (ratings.empty()) {
    return 0;
  }
  return accumulate(ratings.begin(), ratings.end(), 0) /
         static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view &text) const {
  if (text.empty()) {
    throw invalid_argument("Query word is empty"s);
  }
  string_view word = text;
  bool is_minus = false;
  if (word[0] == '-') {
    is_minus = true;
    word.remove_prefix(1);
  }
  string w = string(word);
  if (word.empty() || word[0] == '-' || !IsValidWord(w)) {
    throw invalid_argument("Query word "s + text.data() + " is invalid");
  }
  return {word, is_minus, IsStopWord(w)};
}

SearchServer::Query SearchServer::ParseQuery(const string_view &text,
                                             bool skip_sort) const {
  Query result;
  for (const auto &word : SplitIntoWords(text)) {
    const auto query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        result.minus_words.push_back(query_word.data);
      } else {
        result.plus_words.push_back(query_word.data);
      }
    }
  }
  if (!skip_sort) {
    for (auto *words : {&result.plus_words, &result.minus_words}) {
      sort(words->begin(), words->end());
      words->erase(unique(words->begin(), words->end()), words->end());
    }
  }
  return result;
}


double SearchServer::ComputeWordInverseDocumentFreq(const string_view &word) const {
  return log(GetDocumentCount() * 1.0 /
             word_to_document_freqs_.at(string(word)).size());
}

int SearchServer::GetDocumentCount() const { return documents_.size(); }

std::set<int>::const_iterator SearchServer::begin() const {
  return document_ids_.begin();
}
std::set<int>::const_iterator SearchServer::end() const {
  return document_ids_.end();
}

map<string_view, double> SearchServer::result = map<string_view, double>();

const map<string_view, double>&
SearchServer::GetWordFrequencies(int document_id) const {
  result.clear();
  auto it = doc_to_words_freqs_.find(document_id);
  if (doc_to_words_freqs_.end() != it) {
    for (const auto &w : it->second) {
      const auto &p = word_to_document_freqs_.at(w);
      result[w] += p.at(document_id);
    }
  }
  auto &t = result;
  return t;
}

 void SearchServer::RemoveDocument(int document_id) {
  auto doc_it = find(document_ids_.begin(), document_ids_.end(), document_id);
  if (document_ids_.end() == doc_it) {
    return;
  }
  auto it = doc_to_words_freqs_.find(document_id);
  if (doc_to_words_freqs_.end() != it) {
    for (auto w : it->second) {
      auto t = word_to_document_freqs_[w];
      word_to_document_freqs_[w].erase(document_id);
      if (word_to_document_freqs_[w].empty()) {
        word_to_document_freqs_.erase(w);
      }
    }
    doc_to_words_freqs_.erase(document_id);
  }
  document_ids_.erase(doc_it);
  documents_.erase(document_id);
 }

 void SearchServer::RemoveDocument(std::execution::sequenced_policy policy,
                                  int document_id) {
  RemoveDocument(document_id);
}

 void SearchServer::RemoveDocument(std::execution::parallel_policy policy,
                                  int document_id) {
  auto doc_it = std::find(policy, document_ids_.begin(), document_ids_.end(),
                          document_id);
  if (document_ids_.end() == doc_it) {
    return;
  }
  

  auto it = doc_to_words_freqs_.find(document_id);
  /* std::vector<const string *> words_for_erase(it->second.size());
  std::transform(std::execution::par, it->second.begin(), it->second.end(),
                 words_for_erase.begin(),
                 [](const string &word) { return &(word); });*/
  std::for_each(std::execution::par, it->second.begin(), it->second.end(),
                [&](const auto word) {
                  word_to_document_freqs_[word].erase(document_id);
                });
  document_ids_.erase(doc_it);
  documents_.erase(document_id);
 }