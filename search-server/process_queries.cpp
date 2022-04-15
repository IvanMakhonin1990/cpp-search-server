#include <execution>
#include <algorithm>

#include "process_queries.h"

using namespace std;

vector<vector<Document>>
ProcessQueries(const SearchServer &search_server,
               const vector<string> &queries) {
  vector<vector<Document>> result(queries.size());
  transform(execution::par, queries.begin(), queries.end(), result.begin(),
            [&search_server](std::string query) {
              return search_server.FindTopDocuments(query);
            });
  return result;
  /*for (const std::string &query : queries) {
    documents_lists.push_back(search_server.FindTopDocuments(query));
  }*/
}