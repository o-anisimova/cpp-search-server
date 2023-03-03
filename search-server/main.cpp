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
const double ELIPSON = 1e-6;

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

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document()
        : id(0)
        , relevance(0.0)
        , rating(0) {
    }

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id;
    double relevance;
    int rating;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
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
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        :stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
        AreValidWords(stop_words_);
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text)){
    }

    int GetDocumentId(int index) const {
        if (index < 0 || index >= GetDocumentCount()) {
            throw out_of_range("Invalid document ID"s);
       }

        return document_ids_[index];
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {

        if (documents_.count(document_id) > 0) {
            throw invalid_argument("Document has already been added"s);
        } if (document_id < 0) {
            throw invalid_argument("Document ID is negative"s);
        } if (!(IsValidWord(document))) {
            throw invalid_argument("Document text contains special characters"s);
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document>  FindTopDocuments(const string& raw_query,
        DocumentPredicate document_predicate) const {

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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector <int> document_ids_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
        return rating;
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;

        if (!IsValidWord(text)) {
            throw invalid_argument("Query contains special characters"s);
        }
        else if (text[0] == '-' && text.size() == 1) {
            throw invalid_argument("Empty minus-word"s);
        }
        else if (text[0] == '-' && text[1] == '-') {
            throw invalid_argument("Found more than one minus before the word"s);
        }

        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }

        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
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

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
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
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    template <typename StringContainer>
    void AreValidWords(const StringContainer& words) {
        for (const string& word : stop_words_) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Word contains special characters: "s + word);
            }
        }
    }
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}
int main() {
    //Проверяем, что если задать строку с невалидным словом, конструктор выбросит исключение
    {
        const string check_name = "STOP WORDS, STRING: "s;

        try {
            SearchServer search_server("in w\x12ith from"s);
        } catch(invalid_argument& e) {
            cout << check_name << e.what() << endl;
        }
        catch (...) {
            cout << check_name << "Unknown error" << endl;
        }

    }

    //Проверяем, что если задать вектор с невалидным словом, конструктор выбросит исключение
    {
        const string check_name = "STOP WORDS, VECTOR: "s;

        try {
            vector v{ "in"s, "w\x12ith"s, "from"s };
            SearchServer search_server(v);
        }
        catch (invalid_argument& e) {
            cout << check_name << e.what() << endl;
        }
        catch (...) {
            cout << check_name << "Unknown error" << endl;
        }

    }

    //Проверяем, что если задать мн-во с невалидным словом, конструктор выбросит исключение
    {
        const string check_name = "STOP WORDS, SET: "s;
        
        try {
            set s{ "in"s, "w\x12ith"s, "from"s };
            SearchServer search_server(s);
        }
        catch (invalid_argument& e) {
            cout << check_name << e.what() << endl;
        }
        catch (...) {
            cout << check_name << "Unknown error" << endl;
        }

    }

    //НАЧИНАЕМ ПРОВЕРЯТЬ ADD_DOCUMENT
  
    SearchServer search_server("in the"s);
    const vector ratings = { 7, 2, 7 };
    const string document_content = "black cat in the city"s;
    search_server.AddDocument(1, document_content, DocumentStatus::ACTUAL, ratings);

    //Проверяем, что нельзя добавить документ с тем же ИД
    {
        try {
            search_server.AddDocument(1, document_content, DocumentStatus::ACTUAL, ratings);
        }
        catch (invalid_argument& e) {
            cout << e.what() << endl;
        }
        catch (...) {
            cout << "Unknown error" << endl;
        }
    }

    //Проверяем, что нельзя добавить документ с отрицаиельным ИД
    {
        try {
            search_server.AddDocument(-1, document_content, DocumentStatus::ACTUAL, ratings);
        }
        catch (invalid_argument& e) {
            cout << e.what() << endl;
        }
        catch (...) {
            cout << "Unknown error" << endl;
        }
    }

    //Проверяем, что в тексте документа нет спецсимволов
    {
        try {
            search_server.AddDocument(2, "black ca\x12t in the city"s, DocumentStatus::ACTUAL, ratings);
        }
        catch (invalid_argument& e) {
            cout << e.what() << endl;
        }
        catch (...) {
            cout << "Unknown error" << endl;
        }
    }

    //ПРОВЕРЯЕМ FIND_TOP_DOCUMENT
    
    //Наличие более чем одного минуса перед словами
    {
        try {
            search_server.FindTopDocuments("--city"s);
        }
        catch (invalid_argument& e) {
            cout << e.what() << endl;
        }
    }

    //Спецсимволы в запросе
    {
        try {
            search_server.FindTopDocuments("blac\x12k ca\x12t"s);
        }
        catch (invalid_argument& e) {
            cout << e.what() << endl;
        }
    }

    //Отсутствие текста после символа «минус» в поисковом запросе
    {
        try {
            search_server.FindTopDocuments("black -"s);
        }
        catch (invalid_argument& e) {
            cout << e.what() << endl;
        }
    }

}