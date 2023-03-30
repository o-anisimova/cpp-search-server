#pragma once
#include "string_processing.h"
#include "document.h"
#include "log_duration.h"
#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double ELIPSON = 1e-6;
const map<string, double> EMPTY_MAP = {};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const string& stop_words_text);

    set<int>::iterator begin();

    set<int>::iterator end();

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const;
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(const string& raw_query) const;

    int GetDocumentCount() const;

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const;

    const map<string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        map<string, double> word_freqs;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, map<string, double>> document_to_freqs_;
    map<int, DocumentData> documents_;
    set<int> document_ids_;

    bool IsStopWord(const string& word) const;

    vector<string> SplitIntoWordsNoStop(const string& text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const;

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const;

    double ComputeWordInverseDocumentFreq(const string& word) const;

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    static bool IsValidWord(const string& word);

    template <typename StringContainer>
    static void AreValidWords(const StringContainer& words);
};

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    :stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    AreValidWords(stop_words_);
}

template <typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    vector<Document> result = FindAllDocuments(query, document_predicate);

    sort(result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < ELIPSON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });

    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;
}

template <typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    map<int, double> document_to_relevance;

    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }

    return matched_documents;
}

template <typename StringContainer>
void SearchServer::AreValidWords(const StringContainer& words) {
    for (const string& word : words) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word contains special characters: "s + word);
        }
    }
}