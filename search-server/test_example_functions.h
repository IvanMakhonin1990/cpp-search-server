#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <sstream>
#include <random>

#include "search_server.h"
//#include "remove_duplicates.h"
#include "string_processing.h"
#include "log_duration.h"
#include "process_queries.h"


template <typename T, typename U>
std::ostream &operator<<(std::ostream &os, const std::map<T, U> &m_map) {
  if (m_map.empty())
    return os;
  os << "{";
  for (auto m = m_map.begin(); m != --m_map.end(); ++m) {
    os << m->first << ": " << m->second << ", ";
  }
  const auto m = --m_map.end();
  os << m->first << ": " << m->second << "}";
  return os;
}

template <typename T> std::ostream &operator<<(std::ostream &os, const std::set<T> &m_set) {
  if (m_set.empty())
    return os;
  os << "{";
  for (auto m = m_set.begin(); m != --m_set.end(); ++m) {
    os << *m << ", ";
  }
  const auto m = --m_set.end();
  os << *m << "}";
  return os;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &m_vector) {
  if (m_vector.empty())
    return os;
  os << "[";
  for (auto m = m_vector.begin(); m != --m_vector.end(); ++m) {
    os << *m << ", ";
  }
  const auto m = --m_vector.end();
  os << *m << "]";
  return os;
}

std::ostream &operator<<(std::ostream &os, const DocumentStatus &status);

void AssertImpl(bool value, const std::string &expr_str, const std::string &file,
                const std::string &func, unsigned line, const std::string &hint);

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const std::string &t_str,
                     const std::string &u_str, const std::string &file,
                     const std::string &func, unsigned line,
                     const std::string &hint) {
  if (t != u) {
    std::cout << std::boolalpha;
    std::cout << file << "(" << line << "): " << func << ": ";
    std::cout << "ASSERT_EQUAL(" << t_str << ", " << u_str << ") failed: ";
    std::cout << t << " != " << u << ".";
    if (!hint.empty()) {
      std::cout << " Hint: " << hint;
    }
    std::cout << std::endl;
    abort();
  }
}
#define ASSERT_EQUAL(a, b)                                                     \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint)                                          \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T> void RunTestImpl(T &func, const std::string &func_name);

#define ASSERT(expr)                                                           \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint)                                                \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func) RunTestImpl(func, #func)

#define TEST(processor) Test(#processor, processor, search_server, queries)
#define TEST_POLICY(policy) Test(#policy, search_server, queries, execution::policy)

template <typename ExecutionPolicy>
void Test(std::string_view mark, const SearchServer &search_server,
          const std::vector<std::string> &queries, ExecutionPolicy &&policy) {
  LOG_DURATION(std::string(mark));
  double total_relevance = 0;
  for (const std::string_view query : queries) {
    for (const auto &document : search_server.FindTopDocuments(policy, query)) {
      total_relevance += document.relevance;
    }
  }
  std::cout << total_relevance << std::endl;
}

void AddDocument(SearchServer &search_server, int document_id,
                 const std::string &document, DocumentStatus status,
                 const std::vector<int> &ratings, bool skip_assert = true);

void FindTopDocuments(const SearchServer &search_server,
                      const std::string &raw_query, bool skip_assert=true);

void MatchDocuments(const SearchServer &search_server, const std::string &query,
                    bool skip_assert = true);

void TestExcludeStopWordsFromAddedDocumentContent();

void TestAddDocumentContent();

void TestMinusWords();

void TestMatchingDocuments();

void TestRelevanceSort();

void TestRemoveDocument();
void TestFindPerformance();

template <class T> double average(const T &doc3) {
  int s = 0;
  for (const auto &item : doc3) {
    s += item;
  }
  s /= static_cast<int>(doc3.size());
  return s;
}

std::string GenerateWord(std::mt19937 &generator, int max_length);
std::vector<std::string> GenerateDictionary(std::mt19937 &generator, int word_count, int max_length);
std::string GenerateQuery(std::mt19937 &generator,const std::vector<std::string> &dictionary, int max_word_count);
std::vector<std::string> GenerateQueries(std::mt19937 &generator,const std::vector<std::string> &dictionary, int query_count, int max_word_count);

template <typename QueriesProcessor>
void Test(std::string_view mark, QueriesProcessor processor,
          const SearchServer &search_server, const std::vector<std::string> &queries) {
  LOG_DURATION(std::string(mark));
  const auto documents_lists = processor(search_server, queries);
}

void TestAverageValueOfRaiting();
void TestSearchingOfDocumentsByStatus();

void TestCalculateRelevance();

//void TestRemoveDocument();
void PrintDocument(const Document &document);

void TestLambda();



void TestSearchServer();

void TestMatchDocs1();

void TestExceptions();