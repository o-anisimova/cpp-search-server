#include "search_server.h"
#include <numeric>
#include <cmath>
#include <algorithm>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(string_view(stop_words_text)) {
}

SearchServer::SearchServer(string_view stop_words_text)
    :SearchServer(SplitIntoWords(stop_words_text)){
}

set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
    const vector<int>& ratings) {

    //¬алидации
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("Document has already been added"s);
    } 
    if (document_id < 0) {
        throw invalid_argument("Document ID is negative"s);
    } 
    if (!(IsValidWord(document))) {
        throw invalid_argument("Document text contains special characters"s);
    }

    //ƒобавл€ем документ в хранилище
    storage_.emplace_back(document);

    //–азбиваем документ на слова (с исключением стоп-слов)
    //string_view в векторе уже ссылаетс€ на документ из хранилища
    const vector<string_view> words = SplitIntoWordsNoStop(storage_.back());
    const double inv_word_count = 1.0 / words.size();
    map<string_view, double> word_freqs;
    for (string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs[word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, word_freqs });
    document_ids_.insert(document_id);
}

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, 
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        }
    );
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy seq, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy par, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(par, raw_query, 
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        }
    );
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy seq, string_view raw_query) const {
    return FindTopDocuments(raw_query);
}

vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy par, string_view raw_query) const {
    return FindTopDocuments(par, raw_query, DocumentStatus::ACTUAL);
}


int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw out_of_range("Invalid document ID"s);
    }

    const Query query = ParseQuery(raw_query);

    bool are_minus_words_existed = false;

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            are_minus_words_existed = true;
            break;
        }
    }

    vector<string_view> matched_words;
    if (!are_minus_words_existed) {
        for (string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }    
    }

    return { matched_words, documents_.at(document_id).status };
}

std::tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy seq, string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy par, string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw out_of_range("Invalid document ID"s);
    }

    const Query query = ParseQuery(raw_query, false);
    vector<string_view> matched_words;

    auto func = [this, document_id](string_view word) { 
        if (word_to_document_freqs_.count(word) == 0) {
            return false;
        }
        return word_to_document_freqs_.at(word).count(document_id) > 0; 
    };

    bool are_minus_words_existed = any_of(par, query.minus_words.begin(), query.minus_words.end(), func);

    if (!are_minus_words_existed) {
        matched_words.resize(query.plus_words.size());
        auto it = copy_if(par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), func);
        matched_words.resize(distance(matched_words.begin(), it));
    
        sort(par,matched_words.begin(), matched_words.end());
        auto to_delete = unique(par,matched_words.begin(), matched_words.end());
        matched_words.erase(to_delete, matched_words.end());
    
    }

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
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

SearchServer::Query SearchServer::ParseQuery(string_view text, bool remove_duplicates) const {
    Query query;

    for (string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);

        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    if (remove_duplicates) {
        sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());

        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());
    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (documents_.count(document_id) > 0) {
        return documents_.at(document_id).word_freqs;
    }
    
    static map<string_view, double> empty_map = {};
    return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    for (const auto& [word, freq] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy seq, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy par, int document_id) {
    const map<string_view, double>& word_freqs = GetWordFrequencies(document_id);
    vector<string_view> words(word_freqs.size());
    
    transform(par, word_freqs.begin(), word_freqs.end(), words.begin(), [](auto& word_freq) { return word_freq.first; });
    for_each(par, words.begin(), words.end(), [&](string_view word) { word_to_document_freqs_.at(word).erase(document_id); });

    documents_.erase(document_id);
    document_ids_.erase(find(par, document_ids_.begin(), document_ids_.end(), document_id));
}