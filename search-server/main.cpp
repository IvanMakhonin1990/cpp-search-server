#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

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
  Document() = default;

  Document(int id, double relevance, int rating)
      : id(id), relevance(relevance), rating(rating) {}

  int id = 0;
  double relevance = 0.0;
  int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer &strings) {
  set<string> non_empty_strings;
  for (const string &str : strings) {
    if (!str.empty()) {
      non_empty_strings.insert(str);
    }
  }
  return non_empty_strings;
}

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};

class SearchServer {
public:
  inline static constexpr int INVALID_DOCUMENT_ID = -1;
  inline static constexpr double TOLERANCE = 1.0e-6;

  template <typename StringContainer>
  explicit SearchServer(const StringContainer &stop_words)
      : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    for (const auto &word : stop_words_) {
      if (!IsValidWord(word))
        throw invalid_argument("Word: '" + word + "' in stop words is incorrect");
    }
  }

  explicit SearchServer(const string &stop_words_text)
      : SearchServer(SplitIntoWords(stop_words_text))

  {}

  void AddDocument(int document_id, const string &document,
                   DocumentStatus status, const vector<int> &ratings) {
    if ((document_id < 0)) {
      throw invalid_argument("Id of document is negative");
    }
    if ((documents_.count(document_id) > 0)) {
      throw invalid_argument(string("Document with id = %d is already exist"s, document_id));
    }
    vector<string> words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string &word : words) {
      word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id,
                       DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
  }

  template <typename DocumentPredicate>
  vector<Document>
  FindTopDocuments(const string &raw_query,
                   DocumentPredicate document_predicate) const {
    Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
           if (abs(lhs.relevance - rhs.relevance) < TOLERANCE) {
             return lhs.rating > rhs.rating;
           } else {
             return lhs.relevance > rhs.relevance;
           }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    // Exchange matched_documents and result instead of deep copying
    return matched_documents;
  }

  vector<Document> FindTopDocuments(const string &raw_query,
                                    DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status,
                            int rating) { return document_status == status; });
  }

  vector<Document> FindTopDocuments(const string &raw_query) const {
    return FindTopDocuments(
        raw_query, [](int document_id, DocumentStatus status, int rating) {
          return status == DocumentStatus::ACTUAL;
        });
  }

  int GetDocumentCount() const { return documents_.size(); }

  int GetDocumentId(int index) const {
    if (index >= 0 && index < GetDocumentCount()) {
      return document_ids_[index];
    }
    throw out_of_range("Index of document is outside of documents range");
  }

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
  const set<string> stop_words_;
  map<string, map<int, double>> word_to_document_freqs_;
  map<int, DocumentData> documents_;
  vector<int> document_ids_;

  bool IsStopWord(const string &word) const {
    return stop_words_.count(word) > 0;
  }

  static bool IsValidWord(const string &word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(),
                   [](char c) { return c >= '\0' && c < ' '; });
  }
  bool SplitIntoWordsNoStop(const string &text, vector<string> &result) const {
    result.clear();
    vector<string> words;
    for (const string &word : SplitIntoWords(text)) {
      if (!IsValidWord(word)) {
        throw invalid_argument(
            "A valid word must not contain special characters");
      }
      if (!IsStopWord(word)) {
        words.push_back(word);
      }
    }
    result.swap(words);
    return true;
  }

  vector<string> SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const string &word : SplitIntoWords(text)) {
      if (!IsValidWord(word)) {
        throw invalid_argument(
            "A valid word must not contain special characters");
      }
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
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
  }

  struct QueryWord {
    string data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(string text) const {
    // Empty result by initializing it with default constructed QueryWord

    if (text.empty()) {
      throw invalid_argument("Empty minus word");
    }
    bool is_minus = false;
    if (text[0] == '-') {
      is_minus = true;
      text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
      throw invalid_argument("Invalid symbols in query");
    }

    return {text, is_minus, IsStopWord(text)};
  }

  struct Query {
    set<string> plus_words;
    set<string> minus_words;
  };

  Query ParseQuery(const string &text) const {
    // Empty result by initializing it with default constructed Query
    Query result;
    for (const string &word : SplitIntoWords(text)) {
      QueryWord query_word = ParseQueryWord(word);
      if (!query_word.is_stop) {
        if (query_word.is_minus) {
          result.minus_words.insert(query_word.data);
        } else {
          result.plus_words.insert(query_word.data);
        }
      }
    }
    return result;
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
        if (document_filter(document_id, item.status, item.rating))
          matched_documents.push_back({document_id, relevance, item.rating});
      } else
        cout << "Document data for document with id = " << document_id
             << "is missing" << endl;
    }
    return matched_documents;
  }
};

// ------------ ?????? ????????????? ----------------

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

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings) {
  try {
    search_server.AddDocument(document_id, document, status, ratings);
  } catch (const exception &e) {
    cout << "?????? ?????????? ????????? "s << document_id << ": "s << e.what()
         << endl;
  }
}

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query) {
  cout << "?????????? ?????? ?? ???????: "s << raw_query << endl;
  try {
    for (const Document &document : search_server.FindTopDocuments(raw_query)) {
      PrintDocument(document);
    }
  } catch (const exception &e) {
    cout << "?????? ??????: "s << e.what() << endl;
  }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
  try {
    cout << "??????? ?????????? ?? ???????: "s << query << endl;
    const int document_count = search_server.GetDocumentCount();
    for (int index = 0; index < document_count; ++index) {
      const int document_id = search_server.GetDocumentId(index);
      const auto [words, status] =
          search_server.MatchDocument(query, document_id);
      PrintMatchDocumentResult(document_id, words, status);
    }
  } catch (const exception &e) {
    cout << "?????? ???????? ?????????? ?? ?????? "s << query << ": "s
         << e.what() << endl;
  }
}

int main() {
  SearchServer search_server("? ? ??"s);

  AddDocument(search_server, 1, "???????? ??? ???????? ?????"s,
              DocumentStatus::ACTUAL, {7, 2, 7});
  AddDocument(search_server, 1, "???????? ??? ? ?????? ???????"s,
              DocumentStatus::ACTUAL, {1, 2});
  AddDocument(search_server, -1, "???????? ??? ? ?????? ???????"s,
              DocumentStatus::ACTUAL, {1, 2});
  AddDocument(search_server, 3, "??????? ??? ????\x12??? ???????"s,
              DocumentStatus::ACTUAL, {1, 3, 2});
  AddDocument(search_server, 4, "??????? ??? ??????? ???????"s,
              DocumentStatus::ACTUAL, {1, 1, 1});

  FindTopDocuments(search_server, "???????? -???"s);
  FindTopDocuments(search_server, "???????? --???"s);
  FindTopDocuments(search_server, "???????? -"s);

  MatchDocuments(search_server, "???????? ???"s);
  MatchDocuments(search_server, "?????? -???"s);
  MatchDocuments(search_server, "?????? --???"s);
  MatchDocuments(search_server, "???????? - ?????"s);
}