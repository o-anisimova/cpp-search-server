#include "search_server.h"
#include <numeric>
#include <cmath>
#include <algorithm>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {

    if (documents_.count(document_id) > 0) {
        throw invalid_argument("Document has already been added"s);
    } 
    if (document_id < 0) {
        throw invalid_argument("Document ID is negative"s);
    } 
    if (!(IsValidWord(document))) {
        throw invalid_argument("Document text contains special characters"s);
    }

    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    map<string, double> word_freqs;
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs[word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, word_freqs });
    document_ids_.insert(document_id);
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    vector<string> matched_words;
    const Query query = ParseQuery(raw_query);

    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    return rating;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;

    if (!IsValidWord(text)) {
        throw invalid_argument("Query contains special characters"s);
    }

    if (text[0] == '-') {
        if (text.size() == 1) {
            throw invalid_argument("Empty minus-word"s);
        }
        else if (text[0] == '-' && text[1] == '-') {
            throw invalid_argument("Found more than one minus before the word"s);
        }

        is_minus = true;
        text = text.substr(1);
    }

    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;

    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);

        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (documents_.count(document_id) > 0) {
        return documents_.at(document_id).word_freqs;
    }
    
    static map<string, double> empty_map = {};
    return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    for (const auto& [word, freq] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    documents_.erase(document_id);
    document_ids_.erase(document_id);
}