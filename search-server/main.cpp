#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
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
};

class SearchServer {
public:
    void SetDocumentCount(const int count){
        document_count_ = count;
    }
    
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words= SplitIntoWordsNoStop(document);
        
	const double word_weight = 1.0/words.size();
		
        for (const string& word : words){
            word_to_document_freqs_[word][document_id] += word_weight;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    
    struct Query{
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, map<int, double>> word_to_document_freqs_;
    set<string> stop_words_;
    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        
        return words;
    }

    Query ParseQuery(const string& text) const {
        set<string> plus_words;
        set<string> minus_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-'){
                minus_words.insert(word.substr(1));
            } else {
                if (minus_words.count(word) == 0){
                    plus_words.insert(word);
                }
            }
            
        }
        return {plus_words, minus_words};
    }
	
	double CalculateIdf (const string& word) const {
		return log(document_count_/static_cast<double>(word_to_document_freqs_.at(word).size()));
	}

    vector<Document> FindAllDocuments(const Query& query_words) const {
        map<int, double> document_relevance;
        
        //пробежим по множеству плюс-слов
        for (const string& word : query_words.plus_words){
            
            //проверим, что такое слово есть в словаре
            if (word_to_document_freqs_.count(word) > 0){
                
		const double idf = CalculateIdf(word);
                
                for (const auto&[doc_id, tf] : word_to_document_freqs_.at(word)){
                    document_relevance[doc_id] += idf*tf;
                }   
            }
        }
        
        //теперь исключим минус-слова
        for (const string& word : query_words.minus_words){
            //проверим, что такое слово есть в индексе
            if (word_to_document_freqs_.count(word) > 0){
                for (const auto&[doc_id, tf] : word_to_document_freqs_.at(word)){
                    document_relevance.erase(doc_id);
                }
            }
        }
        
        vector<Document> matched_documents;
       
        //копируем результат в вектор
        for (const auto&[id, relevance] : document_relevance){
            matched_documents.push_back({id, relevance});
        }
        
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
    
    const int document_count = ReadLineWithNumber();
    search_server.SetDocumentCount(document_count);
    
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}