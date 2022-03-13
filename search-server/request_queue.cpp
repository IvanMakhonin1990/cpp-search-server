#include "request_queue.h"
using namespace std;

RequestQueue::RequestQueue(const SearchServer &search_server)
    : m_search_server(search_server) {}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query,
                                DocumentStatus status) {
  auto result = m_search_server.FindTopDocuments(raw_query, status);
  ProcessQueue(requests_);
  requests_.push_back({0, result.empty()});
  return result;
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query) {
  auto result = m_search_server.FindTopDocuments(raw_query);
  ProcessQueue(requests_);
  requests_.push_back({0, result.empty()});
  return result;
}

int RequestQueue::GetNoResultRequests() const {
  int count = 0;
  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    if (it->empty) {
      ++count;
    }
  }
  return count;
}

void RequestQueue::ProcessQueue(deque<QueryResult> &requests) {
  if (requests.empty()) {
    return;
  }
  auto item = requests.front();
  ++item.time;
  requests.pop_front();
  ProcessQueue(requests);
  if (min_in_day_ > item.time) {
    requests.push_front(item);
  }
}

RequestQueue::QueryResult::QueryResult(int time_, bool empty_) 
    : time(time_), empty(empty_) {}
