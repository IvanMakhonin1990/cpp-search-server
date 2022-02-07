// search_server_s1_t2_v2.cpp

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}

int ReadLineWithNumber() {
  int result;
  cin >> result;
  ReadLine();
  return result;
}

vector<string> SplitIntoWords(const string &text) {
  vector<string> words;
  string word;
  for (const char c : text) {
    if (c == ' ') {
      if (!word.empty()) {
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

struct Document {
  int id;
  double relevance;
  int rating;
};

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};

class SearchServer {
public:
  void SetStopWords(const string &text) {
    for (const string &word : SplitIntoWords(text)) {
      stop_words_.insert(word);
    }
  }

  void AddDocument(int document_id, const string &document,
                   DocumentStatus status, const vector<int> &ratings) {
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string &word : words) {
      word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id,
                       DocumentData{ComputeAverageRating(ratings), status});
  }
  
  vector<Document> FindTopDocuments(const string &raw_query) const {
    return FindTopDocuments(
        raw_query, [](int document_id, DocumentStatus status, int rating) {
          return status == DocumentStatus::ACTUAL;
        });
  }

  vector<Document> FindTopDocuments(const string &raw_query,
                                    DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status,
                            int rating) { return document_status == status; });
  }

  template <typename DocumentFilter>
  vector<Document> FindTopDocuments(const string &raw_query,
                                    DocumentFilter document_filter) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_filter);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
           if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
             return lhs.rating > rhs.rating;
           } else {
             return lhs.relevance > rhs.relevance;
           }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);

    return matched_documents;
  }

  int GetDocumentCount() const { return documents_.size(); }

  tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                      int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.push_back(word);
      }
    }
    for (const string &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.clear();
        break;
      }
    }
    return {matched_words, documents_.at(document_id).status};
  }

private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };

  set<string> stop_words_;
  map<string, map<int, double>> word_to_document_freqs_;
  map<int, DocumentData> documents_;

  bool IsStopWord(const string &word) const {
    return stop_words_.count(word) > 0;
  }

  vector<string> SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const string &word : SplitIntoWords(text)) {
      if (!IsStopWord(word)) {
        words.push_back(word);
      }
    }
    return words;
  }

  static int ComputeAverageRating(const vector<int> &ratings) {
    if (ratings.empty()) {
      return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
      rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
  }

  struct QueryWord {
    string data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
      is_minus = true;
      text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
  }

  struct Query {
    set<string> plus_words;
    set<string> minus_words;
  };

  Query ParseQuery(const string &text) const {
    Query query;
    for (const string &word : SplitIntoWords(text)) {
      const QueryWord query_word = ParseQueryWord(word);
      if (!query_word.is_stop) {
        if (query_word.is_minus) {
          query.minus_words.insert(query_word.data);
        } else {
          query.plus_words.insert(query_word.data);
        }
      }
    }
    return query;
  }

  // Existence required
  double ComputeWordInverseDocumentFreq(const string &word) const {
    return log(GetDocumentCount() * 1.0 /
               word_to_document_freqs_.at(word).size());
  }

  template <typename DocumentFilter>
  vector<Document> FindAllDocuments(const Query &query,
                                    DocumentFilter document_filter) const {
    map<int, double> document_to_relevance;
    for (const string &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      for (const auto [document_id, term_freq] :
           word_to_document_freqs_.at(word)) {
        // if (documents_.at(document_id).status == status) {
        document_to_relevance[document_id] += term_freq * inverse_document_freq;
        //}
      }
    }

    for (const string &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
        document_to_relevance.erase(document_id);
      }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
      if (documents_.count(document_id) > 0) {
        const auto &item = documents_.at(document_id);
        if (document_filter(document_id, item.status,
                            item.rating))
          matched_documents.push_back(
              {document_id, relevance, item.rating});
      } else
        cout << "Document data for document with id = " << document_id
             << "is missing" << endl;
    }
    return matched_documents;
  }
};


template <typename T, typename U>
ostream &operator<<(ostream &os, const map<T, U> &m_map) {
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

template <typename T> ostream &operator<<(ostream &os, const set<T> &m_set) {
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
ostream &operator<<(ostream &os, const vector<T> &m_vector) {
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

ostream &operator<<(ostream &os, const DocumentStatus &status) {
  os << static_cast<unsigned int>(status);
  return os;
}

void AssertImpl(bool value, const string &expr_str, const string &file,
                const string &func, unsigned line, const string &hint) {
  if (!value) {
    cerr << file << "("s << line << "): "s << func << ": "s;
    cerr << "Assert("s << expr_str << ") failed."s;
    if (!hint.empty()) {
      cerr << " Hint: "s << hint;
    }
    cerr << endl;
    abort();
  }
}

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str,
                     const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
  if (t != u) {
    cout << boolalpha;
    cout << file << "("s << line << "): "s << func << ": "s;
    cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
    cout << t << " != "s << u << "."s;
    if (!hint.empty()) {
      cout << " Hint: "s << hint;
    }
    cout << endl;
    abort();
  }
}

#define ASSERT_EQUAL(a, b)                                                     \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint)                                          \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T> void RunTestImpl(T &func, const string &func_name) {
  func();
  cerr << func_name << " OK" << endl;
}

#define ASSERT(expr)                                                           \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint)                                                \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func) RunTestImpl(func, #func)

void TestExcludeStopWordsFromAddedDocumentContent() {
  const int doc_id = 42;
  const string content = "cat in the city"s;
  const vector<int> ratings = {1, 2, 3};
  {
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
    const Document &doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);
  }

  {
    SearchServer server;
    server.SetStopWords("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                "Stop words must be excluded from documents"s);
  }
}

void TestAddDocumentContent() {
  {
    SearchServer server;
    ASSERT_EQUAL(0u, server.FindTopDocuments("in"s).size());
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3});
    server.AddDocument(45, "dog at the village", DocumentStatus::ACTUAL,
                       {1, 2, 35});
    server.AddDocument(46, "dog in the village", DocumentStatus::ACTUAL,
                       {1, 2, 3, 30});
    server.AddDocument(46, "dog at the village", DocumentStatus::ACTUAL,
                       {1, 2, 3, 20});
    ASSERT_EQUAL(2u, server.FindTopDocuments("in"s).size());
    server.SetStopWords("the"s);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(2U, found_docs.size());
    ASSERT_EQUAL(46, found_docs.at(0).id);
    ASSERT_EQUAL(42, found_docs.at(1).id);
  }
}

void TestMinusWords() {
  {
    SearchServer server;
    ASSERT(server.FindTopDocuments("in"s).empty());
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3});

    auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(1U, found_docs.size());
    ASSERT_EQUAL(42, found_docs[0].id);

    found_docs = server.FindTopDocuments("in -night"s);
    ASSERT_EQUAL(1U, found_docs.size());
    ASSERT_EQUAL(42, found_docs[0].id);

    found_docs = server.FindTopDocuments("in -the"s);
    ASSERT_EQUAL(0U, found_docs.size());

    server.AddDocument(43, "cat in the city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 4});
    server.AddDocument(66, "cat at the city day"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 4});

    found_docs = server.FindTopDocuments("in -night"s);
    ASSERT_EQUAL(1U, found_docs.size());
    ASSERT_EQUAL(42, found_docs[0].id);
    found_docs = server.FindTopDocuments("in -cat"s);
    ASSERT(found_docs.empty());
  }
}

void TestMatchingDocuments() {
  {
    SearchServer server;

    server.AddDocument(42, "cat in the city"s, DocumentStatus::REMOVED,
                       {1, 2, 3});
    tuple<vector<string>, DocumentStatus> matched_docs =
        server.MatchDocument("in"s, 42);
    ASSERT_EQUAL(1U, get<0>(matched_docs).size());
    ASSERT_EQUAL("in", get<0>(matched_docs)[0]);
    ASSERT_EQUAL(DocumentStatus::REMOVED, get<1>(matched_docs));

    matched_docs = server.MatchDocument("in cat dog"s, 42);
    ASSERT_EQUAL(2U, get<0>(matched_docs).size());
    ASSERT_EQUAL("cat", get<0>(matched_docs)[0]);
    ASSERT_EQUAL("in", get<0>(matched_docs)[1]);
    ASSERT_EQUAL(DocumentStatus::REMOVED, get<1>(matched_docs));

    matched_docs = server.MatchDocument("dog"s, 42);
    ASSERT(get<0>(matched_docs).empty());

    matched_docs = server.MatchDocument("dog  cat in -night"s, 42);
    ASSERT_EQUAL(2U, get<0>(matched_docs).size());
    ASSERT_EQUAL("cat", get<0>(matched_docs)[0]);
    ASSERT_EQUAL("in", get<0>(matched_docs)[1]);
    ASSERT_EQUAL(DocumentStatus::REMOVED, get<1>(matched_docs));

    ASSERT(get<0>(server.MatchDocument("in -city"s, 42)).empty());

    server.AddDocument(43, "cat in cat the city night"s, DocumentStatus::BANNED,
                       {1, 2, 3, 4});
    matched_docs = server.MatchDocument("cat city"s, 43);
    ASSERT_EQUAL(2U, get<0>(matched_docs).size());
    ASSERT_EQUAL("cat", get<0>(matched_docs)[0]);
    ASSERT_EQUAL("city", get<0>(matched_docs)[1]);
    ASSERT_EQUAL(DocumentStatus::BANNED, get<1>(matched_docs));

    ASSERT(get<0>(server.MatchDocument("cat in -city"s, 42)).empty());
  }
}

void TestRelevanceSort() {
  {
    SearchServer server;
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 6});
    server.AddDocument(1, "cat in the in in in in city night"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, 4, 9, 8, 7, 6, 5});
    server.AddDocument(2,
                       "super cat in in in in in in the in in in city night"s,
                       DocumentStatus::ACTUAL,
                       {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
    server.AddDocument(3, "dog in the city"s, DocumentStatus::ACTUAL,
                       {10, 2, 3, 1});
    server.AddDocument(4, "dog in the in in city in night"s,
                       DocumentStatus::ACTUAL, {1, 20, 3, 1, 1, 4, 1, 10});
    server.AddDocument(5, "super dog in city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 40});
    server.AddDocument(6, "super dog city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 30, 4, 5});
    server.AddDocument(5, "super dog in in in city night"s,
                       DocumentStatus::ACTUAL, {1, 2, 30, 40});
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(5U, found_docs.size());
    ASSERT(is_sorted(found_docs.begin(), found_docs.end(),
                     [](const Document &lhs, const Document &rhs) {
                       if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                         return lhs.rating > rhs.rating;
                       } else {
                         return lhs.relevance > rhs.relevance;
                       }
                     }));
  }
}
template <class T> double average(T &doc3) {
  int s = 0;
  for (const auto &item : doc3)
    s += item;
  s /= static_cast<int>(doc3.size());
  return s;
}

void TestRaiting() {
  {
    SearchServer server;
    auto doc0 = {1, 2, 3, 6};
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL, doc0);
    auto doc1 = {1, 2, 3, 4, 9, 8, 7, 6, -5};
    server.AddDocument(1, "cat in the in in in in city night"s,
                       DocumentStatus::ACTUAL, doc1);
    auto doc2 = {1, 2, 3, 4, 5, 6, -7, 8, 9, 10, 11, 12, 13, 14};
    server.AddDocument(2,
                       "super cat in in in in in in the in in in city night"s,
                       DocumentStatus::ACTUAL, doc2);
    auto doc3 = {-10, 2, 3, 1};
    server.AddDocument(3, "dog in the city"s, DocumentStatus::ACTUAL, doc3);
    auto doc4 = {1, 20, 3, 1, 1, 4, 1, 10};
    server.AddDocument(4, "dog in the in in city in night"s,
                       DocumentStatus::ACTUAL, doc4);
    auto doc5 = {1, 2, 3, 40};
    server.AddDocument(5, "super dog in city night"s, DocumentStatus::ACTUAL,
                       doc5);
    auto doc6 = {1, 2, 30, 4, 5};
    server.AddDocument(6, "super dog city night"s, DocumentStatus::ACTUAL,
                       doc6);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(5U, found_docs.size());
    ASSERT_EQUAL(average(doc2), found_docs[0].rating);
  }
}
void TestStatus() {
  {
    SearchServer server;
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 6});
    server.AddDocument(20, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 60});
    server.AddDocument(21, "in in cat in in city1 in in"s,
                       DocumentStatus::ACTUAL, {1, 12, 3, 6});
    server.AddDocument(22, "cat in in in in city2 in in"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, -6});
    server.AddDocument(23, "cat in in in in city3"s, DocumentStatus::ACTUAL,
                       {1, 2, 13, 6});
    server.AddDocument(24, "cat in in in city4"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, -6});
    server.AddDocument(1, "cat in the in in in in city night"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, 4, 9, 8, 7, 6, 5});
    server.AddDocument(2,
                       "super cat in in in in in in the in in in city night"s,
                       DocumentStatus::ACTUAL,
                       {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
    server.AddDocument(3, "dog in the city"s, DocumentStatus::BANNED,
                       {10, 2, 3, 1});
    server.AddDocument(31, "dog in the city2 banned"s, DocumentStatus::BANNED,
                       {10, 2, 3, 1});
    server.AddDocument(32, "dog in the banned city banned"s,
                       DocumentStatus::BANNED, {10, 2, 3, 1});
    server.AddDocument(33, "banned dog banned in the city banned"s,
                       DocumentStatus::BANNED, {10, 2, 3, 1});
    server.AddDocument(
        34,
        "dog banned in banned the banned in banned in bannedcity  in night"s,
        DocumentStatus::ACTUAL, {1, 20, 3, 1, 1, 4, 1, 10});
    server.AddDocument(
        34,
        "dog banned in banned the banned in banned in bannedcity  in night"s,
        DocumentStatus::ACTUAL, {1, 20, 3, 1, 1, 4, 1, 10});
    server.AddDocument(35, "super dog in city night"s, DocumentStatus::BANNED,
                       {1, 2, 3, 40});

    server.AddDocument(6, "super dog city night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(61, "super IRRELEVANT dog IRRELEVANT city night"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(62, "super IRRELEVANT dog city IRRELEVANT night"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(63,
                       "super IRRELEVANT dog city IRRELEVANT night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(64,
                       "super do IRRELEVANTg city IRRELEVANT night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(
        65, "super do IRRELEVANTg city IRRELEVANT night IRRELEVANT cat"s,
        DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});

    auto found_docs =
        server.FindTopDocuments("IRRELEVANT"s, DocumentStatus::REMOVED);
    ASSERT(found_docs.empty());

    found_docs =
        server.FindTopDocuments("IRRELEVANT"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(5U, found_docs.size());
    ASSERT_EQUAL(63, found_docs[0].id);
    ASSERT_EQUAL(61, found_docs[1].id);
    ASSERT_EQUAL(62, found_docs[2].id);
    ASSERT_EQUAL(64, found_docs[3].id);
    ASSERT_EQUAL(65, found_docs[4].id);

    server.AddDocument(7, "super dog city night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(71, "super REMOVED dog REMOVED city night"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(72, "super REMOVED dog city REMOVED night"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(73, "super REMOVED dog city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(74, "super do REMOVED g city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(75, "super do REMOVED g city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(76,
                       "super super do REMOVED g city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    found_docs = server.FindTopDocuments("REMOVED"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(5U, found_docs.size());
    ASSERT_EQUAL(73, found_docs[0].id);
    ASSERT_EQUAL(74, found_docs[1].id);
    ASSERT_EQUAL(75, found_docs[2].id);
    ASSERT_EQUAL(71, found_docs[3].id);
    ASSERT_EQUAL(72, found_docs[4].id);
  }
}

void TestRelevance() {
  {
    SearchServer server;
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 6});
    server.AddDocument(1, "cat in the in in in in city night"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, 4, 9, 8, 7, 6, 5});
    server.AddDocument(2,
                       "super cat in in in in in in the in in in city night"s,
                       DocumentStatus::ACTUAL,
                       {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
    server.AddDocument(3, "dog in the city"s, DocumentStatus::ACTUAL,
                       {10, 2, 3, 1});
    server.AddDocument(4, "dog in the in in city in night"s,
                       DocumentStatus::ACTUAL, {1, 20, 3, 1, 1, 4, 1, 10});
    server.AddDocument(5, "super dog in city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 40});
    server.AddDocument(6, "super dog city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 30, 4, 5});
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(5U, found_docs.size());
    ASSERT(fabs(found_docs[0].relevance - 0.099096865603237494) < 1e-6);

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {9});
    auto r = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT(fabs(r[0].relevance - 0.866434) < 1e-6);
  }
}

void TestFilterByPredicate() {
  {
    SearchServer server;
    ASSERT(server
               .FindTopDocuments(
                   "test",
                   [](int document_id, DocumentStatus status, int rating) {
                     int a = 0;
                     a += document_id;
                     a += rating;

                     return status == DocumentStatus::ACTUAL;
                   })
               .empty());
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 6});
    server.AddDocument(20, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 4, 9, 8, 37, 6, 5});
    server.AddDocument(21, "in in cat in in city1 in in"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, 4, 9, 83, 7, 6, 5});
    server.AddDocument(22, "cat in in in in city2 in in"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, 4, 9, 28, 7, 6, 5});
    server.AddDocument(23, "cat in in in in city3"s, DocumentStatus::ACTUAL,
                       {1, 23, 3, 4, 9, 8, 7, 6, 5});
    server.AddDocument(24, "cat in in in city4"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 4, 9, 8, 7, 61, 5});
    server.AddDocument(1, "cat in the in in in in city night"s,
                       DocumentStatus::ACTUAL, {1, 2, 3, 4, 9, 8, 7, 6, 5});
    server.AddDocument(2,
                       "super cat in in in in in in the in in in city night"s,
                       DocumentStatus::ACTUAL,
                       {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
    server.AddDocument(3, "dog in the city"s, DocumentStatus::BANNED,
                       {10, 2, 3, 1});
    server.AddDocument(31, "dog in the city2 banned"s, DocumentStatus::BANNED,
                       {10, 2, 3, 1});
    server.AddDocument(32, "dog in the banned city banned"s,
                       DocumentStatus::BANNED, {10, 2, 3, 1});
    server.AddDocument(33, "banned dog banned in the city banned"s,
                       DocumentStatus::BANNED, {10, 2, 3, 1});
    server.AddDocument(
        34,
        "dog banned in banned the banned in banned in bannedcity  in night"s,
        DocumentStatus::ACTUAL, {1, 20, 3, 1, 1, 4, 1, 10});
    server.AddDocument(
        34,
        "dog banned in banned the banned in banned in bannedcity  in night"s,
        DocumentStatus::ACTUAL, {1, 20, 3, 1, 1, 4, 1, 10});
    server.AddDocument(35, "super dog in city night"s, DocumentStatus::BANNED,
                       {1, 2, 3, 40});

    server.AddDocument(6, "super dog city night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, -300, 4, 5});
    server.AddDocument(61, "super IRRELEVANT dog IRRELEVANT city night"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(62, "super IRRELEVANT dog city IRRELEVANT night"s,
                       DocumentStatus::IRRELEVANT, {1, 2, -30000, 4, 5});
    server.AddDocument(63,
                       "super IRRELEVANT dog city IRRELEVANT night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 300, 4, 5});
    server.AddDocument(64,
                       "super do IRRELEVANTg city IRRELEVANT night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(
        65, "super do IRRELEVANTg city IRRELEVANT night IRRELEVANT cat"s,
        DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});

    server.AddDocument(164,
                       "super do IRRELEVANTg city IRRELEVANT night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
    server.AddDocument(264,
                       "super do IRRELEVANTg city IRRELEVANT night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});

    auto found_docs = server.FindTopDocuments(
        "IRRELEVANT", [](int document_id, DocumentStatus status, int rating) {
          return status == DocumentStatus::IRRELEVANT && rating < 0 &&
                 document_id > 100;
        });
    ASSERT(found_docs.empty());

    found_docs = server.FindTopDocuments(
        "IRRELEVANT", [](int document_id, DocumentStatus status, int rating) {
          return status == DocumentStatus::IRRELEVANT && rating < 0 &&
                 document_id > 60 && document_id < 70;
        });
    ASSERT_EQUAL(1U, found_docs.size());
    ASSERT_EQUAL(62, found_docs[0].id);
    ASSERT_EQUAL(-5997, found_docs[0].rating);
    ASSERT(fabs(0.337200 - found_docs[0].relevance) < 1.0e-6);

    server.AddDocument(7, "super dog city night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(71, "super REMOVED dog REMOVED city night"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(72, "super REMOVED dog city REMOVED night"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(73, "super REMOVED dog city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(74, "super do REMOVED g city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(75, "super do REMOVED g city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    server.AddDocument(76,
                       "super super do REMOVED g city REMOVED night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    found_docs = server.FindTopDocuments(
        "REMOVED"s, [](int document_id, DocumentStatus status, int rating) {
          return status == DocumentStatus::REMOVED && rating > 0 &&
                 document_id > 70;
        });
    ASSERT_EQUAL(5U, found_docs.size());
    ASSERT_EQUAL(73, found_docs[0].id);
  }
}

void TestSearchServer() {
  RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
  RUN_TEST(TestAddDocumentContent);
  RUN_TEST(TestMinusWords);
  RUN_TEST(TestMatchingDocuments);
  RUN_TEST(TestRelevanceSort);
  RUN_TEST(TestRaiting);
  RUN_TEST(TestStatus);
  RUN_TEST(TestRelevance);
  RUN_TEST(TestFilterByPredicate);
}


int main() {
  TestSearchServer();
  cout << "Search server testing finished"s << endl;
}