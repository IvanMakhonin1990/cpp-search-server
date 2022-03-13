#pragma once

#include "search_server.h"
#include "document.h"

using namespace std;


void PrintDocument(const Document &document);

void PrintMatchDocumentResult(int document_id, const vector<string> &words,
                              DocumentStatus status);

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings);

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query);

void MatchDocuments(const SearchServer &search_server, const string &query);