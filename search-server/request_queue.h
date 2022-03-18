#pragma once
#include <deque>
#include "search_server.h"

class RequestQueue {
public:
  explicit RequestQueue(const SearchServer &search_server);
  template <typename DocumentPredicate>
  std::vector<Document> AddFindRequest(const std::string &raw_query,
                                       DocumentPredicate document_predicate);

  std::vector<Document> AddFindRequest(const std::string &raw_query,
                                  DocumentStatus status);

  std::vector<Document> AddFindRequest(const std::string &raw_query);

  int GetNoResultRequests() const;

private:
  struct QueryResult {
    int time;
    bool empty;
    QueryResult(int time_, bool empty_);
  };
  std::deque<QueryResult> requests_;
  const static int min_in_day_ = 1440;
  const SearchServer &m_search_server;
  void ProcessQueue(std::deque<QueryResult> &requests);
};

template <typename DocumentPredicate>
std::vector<Document>
RequestQueue::AddFindRequest(const std::string &raw_query,
                             DocumentPredicate document_predicate) {
  auto result = m_search_server.FindTopDocuments(raw_query, document_predicate);
  ProcessQueue(requests_);
  requests_.push_back({0, result.empty()});
  return result;
}