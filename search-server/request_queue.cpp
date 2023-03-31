#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(SearchServer& search_server)
    :search_server_(search_server), empty_requests_cnt_(0){
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    vector<Document> document_list = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(raw_query, document_list);
    return document_list;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    vector<Document> document_list = search_server_.FindTopDocuments(raw_query);
    AddRequest(raw_query, document_list);
    return document_list;
}
    
int RequestQueue::GetNoResultRequests() const {
    return empty_requests_cnt_;
}

RequestQueue::QueryResult::QueryResult(const string& query, const bool is_query_result_empty)
    :query_(query), is_query_result_empty_(is_query_result_empty) {
}


void RequestQueue::AddRequest(const string& raw_query, const vector<Document>& document_list) {
    QueryResult query_result(raw_query, document_list.empty());

    if (requests_.size() == 1440) {
        if (requests_.front().is_query_result_empty_) {
            --empty_requests_cnt_;
        }
        requests_.pop_front();
    }

    requests_.push_back(query_result);
    if (query_result.is_query_result_empty_) {
        ++empty_requests_cnt_;
    }
}