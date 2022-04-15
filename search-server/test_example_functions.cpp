#include "test_example_functions.h"

using namespace std;

std::ostream &operator<<(std::ostream &os, const DocumentStatus &status)
{
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


template <typename T> void RunTestImpl(T &func, const string &func_name) {
  func();
  cerr << func_name << " OK" << endl;
}

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings) {
  try {
    search_server.AddDocument(document_id, document, status, ratings);
  } catch (const invalid_argument &e) {
    cout << "Ошибка добавления документа "s << document_id << ": "s << e.what()
         << endl;
  }
}

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query) {
  cout << "Результаты поиска по запросу: "s << raw_query << endl;
  try {
    for (const Document &document : search_server.FindTopDocuments(raw_query)) {
      PrintDocument(document);
    }
  } catch (const invalid_argument &e) {
    cout << "Ошибка поиска: "s << e.what() << endl;
  }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
  try {
    cout << "Матчинг документов по запросу: "s << query << endl;
    const int document_count = search_server.GetDocumentCount();
    for (int index = 0; index < document_count; ++index) {
      const int document_id =
          *find(search_server.begin(), search_server.end(), index);
      const auto [words, status] =
          search_server.MatchDocument(query, document_id);
      PrintMatchDocumentResult(document_id, words, status);
    }
  } catch (const invalid_argument &e) {
    cout << "Ошибка матчинга документов на запрос "s << query << ": "s
         << e.what() << endl;
  }
}

string GenerateWord(mt19937 &generator, int max_length) {
  const int length = 0;
  uniform_int_distribution(1, max_length)(generator);
  string word;
  word.reserve(length);
  for (int i = 0; i < length; ++i) {
    word.push_back(uniform_int_distribution(static_cast<int>('a'),
                                            static_cast<int> ('z'))(generator));
  }
  return word;
}

vector<string> GenerateDictionary(mt19937 &generator, int word_count,
                                  int max_length) {
  vector<string> words;
  words.reserve(word_count);
  for (int i = 0; i < word_count; ++i) {
    words.push_back(GenerateWord(generator, max_length));
  }
  sort(words.begin(), words.end());
  words.erase(unique(words.begin(), words.end()), words.end());
  return words;
}

string GenerateQuery(mt19937 &generator, const vector<string> &dictionary,
                     int max_word_count) {
  const int word_count = uniform_int_distribution(1, max_word_count)(generator);
  string query;
  for (int i = 0; i < word_count; ++i) {
    if (!query.empty()) {
      query.push_back(' ');
    }
    query += dictionary[uniform_int_distribution<int>(0, static_cast<int>(dictionary.size() -
                                                            1))(generator)];
  }
  return query;
}

vector<string> GenerateQueries(mt19937 &generator,
                               const vector<string> &dictionary,
                               int query_count, int max_word_count) {
  vector<string> queries;
  queries.reserve(query_count);
  for (int i = 0; i < query_count; ++i) {
    queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
  }
  return queries;
}



void TestExcludeStopWordsFromAddedDocumentContent() {
  const int doc_id = 42;
  const string content = "cat in the city"s;
  const vector<int> ratings = {1, 2, 3};
  {
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
    const Document &doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);
  }

  {
    SearchServer server("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                "Stop words must be excluded from documents"s);
  }
}

void TestAddDocumentContent() {
  {
    {
      SearchServer server(""s);
      ASSERT_EQUAL(0u, server.FindTopDocuments("in"s).size());
      server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL,
                         {1, 2, 3});
      server.AddDocument(45, "dog at the village", DocumentStatus::ACTUAL,
                         {1, 2, 35});
      server.AddDocument(46, "dog in the village", DocumentStatus::ACTUAL,
                         {1, 2, 3, 30});
      server.AddDocument(47, "dog at the village", DocumentStatus::ACTUAL,
                         {1, 2, 3, 20});
      ASSERT_EQUAL(2u, server.FindTopDocuments("in"s).size());
    }
    {
      SearchServer server("the"s);
      server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL,
                         {1, 2, 3});
      server.AddDocument(45, "dog at the village", DocumentStatus::ACTUAL,
                         {1, 2, 35});
      server.AddDocument(46, "dog in the village", DocumentStatus::ACTUAL,
                         {1, 2, 3, 30});
      server.AddDocument(47, "dog at the village", DocumentStatus::ACTUAL,
                         {1, 2, 3, 20});
      const auto found_docs = server.FindTopDocuments("in"s);
      ASSERT_EQUAL(2U, found_docs.size());
      ASSERT_EQUAL(46, found_docs.at(0).id);
      ASSERT_EQUAL(42, found_docs.at(1).id);
    }
  }
}

void TestMinusWords() {
  {
    SearchServer server(""s);
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
    SearchServer server(""s);

    server.AddDocument(42, "cat in the city"s, DocumentStatus::REMOVED,
                       {1, 2, 3});
    // tuple<vector<string>, DocumentStatus> matched_docs =
    {
      auto [words, status] = server.MatchDocument("in"s, 42);
      ASSERT_EQUAL(1U, words.size());
      ASSERT_EQUAL("in", words[0]);
      ASSERT_EQUAL(DocumentStatus::REMOVED, status);
    }

    {
      auto [words, status] = server.MatchDocument("in cat dog"s, 42);
      ASSERT_EQUAL(2U, words.size());
      ASSERT_EQUAL("cat", words[0]);
      ASSERT_EQUAL("in", words[1]);
      ASSERT_EQUAL(DocumentStatus::REMOVED, status);
    }
    ASSERT(get<0>(server.MatchDocument("dog"s, 42)).empty());

    {
      auto [words, status] = server.MatchDocument("dog  cat in -night"s, 42);
      ASSERT_EQUAL(2U, words.size());
      ASSERT_EQUAL("cat", words[0]);
      ASSERT_EQUAL("in", words[1]);
      ASSERT_EQUAL(DocumentStatus::REMOVED, status);
    }

    ASSERT(get<0>(server.MatchDocument("in -city"s, 42)).empty());

    server.AddDocument(43, "cat in cat the city night"s, DocumentStatus::BANNED,
                       {1, 2, 3, 4});
    {
      auto [words, status] = server.MatchDocument("cat city"s, 43);
      ASSERT_EQUAL(2U, words.size());
      ASSERT_EQUAL("cat", words[0]);
      ASSERT_EQUAL("city", words[1]);
      ASSERT_EQUAL(DocumentStatus::BANNED, status);
    }
  }
}

void TestRelevanceSort() {
  {
    SearchServer server(""s);
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
    server.AddDocument(5, "super dog in city night super dog in in in city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 40});
    server.AddDocument(6, "super dog city night"s, DocumentStatus::ACTUAL,
                       {1, 2, 30, 4, 5});
    server.AddDocument(7, "super dog in in in city night"s,
                       DocumentStatus::ACTUAL, {1, 2, 30, 40});
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(5U, found_docs.size());

    ASSERT_EQUAL(2u, found_docs[0].id);
    ASSERT_EQUAL(1u, found_docs[1].id);
    ASSERT_EQUAL(4u, found_docs[2].id);
    ASSERT_EQUAL(0u, found_docs[3].id);
    ASSERT_EQUAL(7u, found_docs[4].id);

    for (int i = 0; i < static_cast<int>(found_docs.size()) - 1; ++i) {
      ASSERT(found_docs[i].relevance >= found_docs[i + 1].relevance);
    }
  }
}

void TestAverageValueOfRaiting() {
  {
    SearchServer server(""s);
    auto doc0 = {1, 2, 3, 6};
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL, doc0);
    auto doc1 = {1, 2, 3, 4, 9, 8, 7, 6, -5};
    server.AddDocument(1, "cat in the in in in in city night"s,
                       DocumentStatus::ACTUAL, doc1);
    auto doc2 = {1, 2, 3, 4, 5, 6, -7, 8, 9, 10, 11, 12, 13, 14};
    server.AddDocument(2,
                       "super cat in in in in in in the in in in city night"s,
                       DocumentStatus::ACTUAL, doc2);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(3u, found_docs.size());
    ASSERT_EQUAL(average(doc2), found_docs[0].rating);
    ASSERT_EQUAL(average(doc1), found_docs[1].rating);
  }
}
void TestSearchingOfDocumentsByStatus() {
  {
    SearchServer server(""s);
    server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL,
                       {1, 2, 3, 6});
    server.AddDocument(3, "dog in the city"s, DocumentStatus::BANNED,
                       {10, 2, 3, 1});
    server.AddDocument(6, "super dog city night IRRELEVANT"s,
                       DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});

    auto found_docs =
        server.FindTopDocuments("IRRELEVANT"s, DocumentStatus::REMOVED);
    ASSERT(found_docs.empty());

    found_docs =
        server.FindTopDocuments("IRRELEVANT"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(1U, found_docs.size());
    ASSERT_EQUAL(6, found_docs[0].id);

    server.AddDocument(7, "super dog city night REMOVED"s,
                       DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
    found_docs = server.FindTopDocuments("REMOVED"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(1u, found_docs.size());
    ASSERT_EQUAL(7, found_docs[0].id);
  }
}

void TestCalculateRelevance() {
  SearchServer server(""s);
  server.AddDocument(2, "super cat in in in in in in the in in in city night"s,
                     DocumentStatus::ACTUAL,
                     {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  server.AddDocument(6, "super dog city night"s, DocumentStatus::ACTUAL,
                     {1, 2, 30, 4, 5});
  const auto found_docs = server.FindTopDocuments("in"s);
  ASSERT_EQUAL(1u, found_docs.size());
  ASSERT(fabs(found_docs[0].relevance - log(2.0 / 1.0) * 9.0 / 14.0) < 1e-6);
}

void TestFilterByPredicate() {
  SearchServer server(""s);
  ASSERT(server
             .FindTopDocuments(
                 "test",
                 [](int document_id, DocumentStatus status, int rating) {
                   return status == DocumentStatus::ACTUAL;
                 })
             .empty());
  server.AddDocument(0, "cat in in city"s, DocumentStatus::ACTUAL,
                     {1, 2, 3, 6});

  server.AddDocument(3, "dog in the city"s, DocumentStatus::BANNED,
                     {10, 2, 3, 1});

  ASSERT(server
             .FindTopDocuments(
                 "IRRELEVANT",
                 [](int document_id, DocumentStatus status, int rating) {
                   return status == DocumentStatus::IRRELEVANT && rating < 0 &&
                          document_id > 100;
                 })
             .empty());

  server.AddDocument(62, "super IRRELEVANT dog city IRRELEVANT night"s,

                     DocumentStatus::IRRELEVANT, {1, 2, -30000, 4, 5});

  auto found_docs = server.FindTopDocuments(
      "IRRELEVANT", [](int document_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::IRRELEVANT && rating < 0 &&
               document_id > 60 && document_id < 70;
      });
  ASSERT_EQUAL(1U, found_docs.size());
  ASSERT_EQUAL(62, found_docs[0].id);
  ASSERT_EQUAL(-5997, found_docs[0].rating);
  ASSERT(fabs(log(3.0 / 1.0) * 2.0 / 6.0 - found_docs[0].relevance) < 1.0e-6);

  server.AddDocument(73, "super REMOVED dog city REMOVED night REMOVED"s,
                     DocumentStatus::REMOVED, {1, 2, 30, 4, 5});
  found_docs = server.FindTopDocuments(
      "REMOVED"s, [](int document_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::REMOVED && rating > 0 &&
               document_id > 70;
      });
  ASSERT_EQUAL(1u, found_docs.size());
  ASSERT_EQUAL(73, found_docs[0].id);
}

//void TestRemoveDocument() {
//  SearchServer server(""s);
//  server.AddDocument(80, "cat in in city"s, DocumentStatus::ACTUAL,
//                     {1, 2, 3, 6});
//  server.AddDocument(70, "cat in in city"s, DocumentStatus::ACTUAL,
//                     {1, 2, 3, 6});
//  server.AddDocument(60, "cat in in city"s, DocumentStatus::ACTUAL,
//                     {1, 2, 3, 6});
//  server.AddDocument(50, "cat in in city"s, DocumentStatus::ACTUAL,
//                     {1, 2, 3, 6});
//  server.AddDocument(40, "dog in the city"s, DocumentStatus::BANNED,
//                     {10, 2, 3, 1});
//  server.AddDocument(30, "super dog city night IRRELEVANT"s,
//                     DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
//  server.AddDocument(20, "super dog city night IRRELEVANT"s,
//                     DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
//  server.AddDocument(10, "super dog city night IRRELEVANT"s,
//                     DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
//  server.AddDocument(0, "super dog city night IRRELEVANT"s,
//                     DocumentStatus::IRRELEVANT, {1, 2, 30, 4, 5});
//  RemoveDuplicates(server);
//
//  auto found_docs =
//      server.FindTopDocuments("IRRELEVANT"s, DocumentStatus::REMOVED);
//  ASSERT(found_docs.empty());
//
//  found_docs =
//      server.FindTopDocuments("IRRELEVANT"s, DocumentStatus::IRRELEVANT);
//  ASSERT_EQUAL(1U, found_docs.size());
//  ASSERT_EQUAL(0, found_docs[0].id);
//}

void TestLambda() {
  const std::vector<int> ratings1 = {1, 2, 3, 4, 5};
  const std::vector<int> ratings2 = {-1, -2, 30, -3, 44, 5};
  const std::vector<int> ratings3 = {12, -20, 80, 0, 8, 0, 0, 9, 67};
  const std::vector<int> ratings4 = {7, 0, 3, -49, 5};
  const std::vector<int> ratings5 = {81, -6, 7, 94, -7};
  const std::vector<int> ratings6 = {41, 8, -7, 897, 5};
  const std::vector<int> ratings7 = {543, 0, 43, 4, -5};
  const std::vector<int> ratings8 = {91, 7, 3, -88, 56};
  const std::vector<int> ratings9 = {0, -87, 93, 66, 5};
  const std::vector<int> ratings10 = {11, 2, -43, 4, 895};
  //const int max_ratings_length = 10;
  //const int max_rating_value = 100;

  string stop_words = "и в на";
  SearchServer lambda_search_server(stop_words);
  lambda_search_server.AddDocument(0, "белый кот и модный ошейник",
                                      DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
   lambda_search_server.AddDocument(1, "пушистый кот пушистый хвост",
                                   DocumentStatus::ACTUAL, ratings2);
  lambda_search_server.AddDocument(2, "ухоженный пёс выразительные глаза",
                                   DocumentStatus::ACTUAL, ratings3);
  lambda_search_server.AddDocument(3, "белый модный кот",
                                   DocumentStatus::IRRELEVANT, ratings4);
  lambda_search_server.AddDocument(4, "пушистый кот пёс",
                                   DocumentStatus::IRRELEVANT, ratings5);
  lambda_search_server.AddDocument(5, "ухоженный ошейник выразительные глаза",
                                   DocumentStatus::IRRELEVANT, ratings6);
  lambda_search_server.AddDocument(6, "кот и ошейник", DocumentStatus::BANNED,
                                   ratings7);
  lambda_search_server.AddDocument(7, "пёс и хвост", DocumentStatus::BANNED,
                                   ratings8);
  lambda_search_server.AddDocument(8, "модный пёс пушистый хвост",
                                   DocumentStatus::BANNED, ratings9);
  lambda_search_server.AddDocument(9, "кот пушистый ошейник",
                                   DocumentStatus::REMOVED, ratings10);
  lambda_search_server.AddDocument(10, "ухоженный кот и пёс",
                                   DocumentStatus::REMOVED, ratings2);
  lambda_search_server.AddDocument(11, "хвост и выразительные глаза",
                                   DocumentStatus::REMOVED, ratings3);
  const string lambda_query = "пушистый ухоженный кот";
  cout << "Ratings > 10 and Id < 7:" << endl;
  const auto documents1 = lambda_search_server.FindTopDocuments(
      lambda_query, [](int document_id, DocumentStatus status, int rating) {
        return rating > 10 && document_id < 7;
      });
  for (const Document &document : documents1) {
    PrintDocument(document);
  }
  cout << "Even documents and ACTUAL:" << endl;
  const auto documents2 = lambda_search_server.FindTopDocuments(
      lambda_query, [](int document_id, DocumentStatus status, int rating) {
        return document_id % 2 == 0 && status == DocumentStatus::ACTUAL;
      });
  for (const Document &document : documents2) {
    PrintDocument(document);
  }
}

void TestParallel() {
  mt19937 generator;
  const auto dictionary = GenerateDictionary(generator, 2000, 25);
  const auto documents = GenerateQueries(generator, dictionary, 20000, 10);

  SearchServer search_server(dictionary[0]);
  for (size_t i = 0; i < documents.size(); ++i) {
    search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL,
                              {1, 2, 3});
  }

  const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
  TEST(ProcessQueries);
} 

void TestParallel1() {
  SearchServer search_server("and with"s);

  int id = 0;
  for (const string &text : {
           "funny pet and nasty rat"s,
           "funny pet with curly hair"s,
           "funny pet and not very nasty rat"s,
           "pet with rat and rat and rat"s,
           "nasty rat with curly hair"s,
       }) {
    search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
  }

  const vector<string> queries = {"nasty rat -not"s,
                                  "not very funny nasty pet"s, "curly hair"s};
  for (const Document &document :
       ProcessQueriesJoined(search_server, queries)) {
    cout << "Document "s << document.id << " matched with relevance "s
         << document.relevance << endl;
  }
}

void TestSearchServer() {
  RUN_TEST(TestLambda);
  //RUN_TEST(TestRemoveDocument);
  RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
  RUN_TEST(TestAddDocumentContent);
  RUN_TEST(TestMinusWords);
  RUN_TEST(TestMatchingDocuments);
  RUN_TEST(TestRelevanceSort);
  RUN_TEST(TestAverageValueOfRaiting);
  RUN_TEST(TestSearchingOfDocumentsByStatus);
  RUN_TEST(TestCalculateRelevance);
  RUN_TEST(TestFilterByPredicate);
  RUN_TEST(TestParallel);
  RUN_TEST(TestParallel1);
}