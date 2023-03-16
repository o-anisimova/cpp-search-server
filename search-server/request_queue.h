#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(SearchServer& search_server);
    
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate);
    
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);
    
    vector<Document> AddFindRequest(const string& raw_query);
    
    int GetNoResultRequests() const;
    
private:
    struct QueryResult {
        string query_;
        bool is_query_result_empty_;

        explicit QueryResult(const string& query, const bool is_query_result_empty);
    };
    
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    SearchServer& search_server_;
    int empty_requests_cnt_;

    void AddRequest(const string& raw_query, const vector<Document>& document_list);
};

template <typename DocumentPredicate>
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
    vector<Document> document_list = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(raw_query, document_list);
    return document_list;
}