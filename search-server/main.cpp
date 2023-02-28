#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */

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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename Function>
    vector<Document> FindTopDocuments(const string& raw_query,
        Function function) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, function);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < ELIPSON) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto function = [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; };
        return FindTopDocuments(raw_query, function);
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
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

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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
        // Word shouldn't be empty
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

    template <typename Function>
    vector<Document> FindAllDocuments(const Query& query, Function function) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                DocumentData document_data = documents_.at(document_id);
                if (function(document_id, document_data.status, document_data.rating)) {
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
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;

        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Term>
void RunTestImpl(Term func, const string& func_str) {
    func();
    cerr << func_str << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl(func, #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/

//Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса, не должны включаться в результаты поиска.
void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // Сначала убеждаемся, что при отсутствии минус-слов документ находится
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что тот же запрос, но с минус словом
    // возвращает пустой результат
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Documents containing minus-words must be excluded in search results"s);
    }
}

//Соответствие документов поисковому запросу.При этом должны быть возвращены все слова из поискового запроса, присутствующие в документе.
//Если есть соответствие хотя бы по одному минус - слову, должен возвращаться пустой список слов.
void TestDocumentMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    //Проверяем список совпавших слов
    {
        const auto& matched_document = server.MatchDocument("cat city"s, doc_id);
        const vector<string>& words = get<vector<string>>(matched_document);

        ASSERT_EQUAL(words.size(), 2);
        ASSERT_EQUAL(count(words.begin(), words.end(), "cat"s), 1);
        ASSERT_EQUAL(count(words.begin(), words.end(), "city"s), 1);
    }

    //Проверяем, что если в документе есть минус-слово, список слов пустой
    {
        const auto& matched_document = server.MatchDocument("cat -city"s, doc_id);
        const vector<string>& words = get<vector<string>>(matched_document);

        ASSERT_EQUAL(words.size(), 0);
    }
}

//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestDocumentsSortByRelevance() {
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;

    //Добавляем несколько документов, которые будем сортировать
    server.AddDocument(0, "black cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "white dog in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "white cat in the street"s, DocumentStatus::ACTUAL, ratings);

    const auto found_docs = server.FindTopDocuments("white cat"s);

    ASSERT_EQUAL(found_docs.size(), 3);
    ASSERT_HINT(found_docs[0].relevance >= found_docs[1].relevance && found_docs[1].relevance >= found_docs[2].relevance, "Documents must be sorted in descending order of relevance"s);
}

//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestComputionDocumentRating() {
    const string content = "black cat in the city"s;

    //Пустой вектор оценок
    {
        SearchServer server;

        server.AddDocument(0, content, DocumentStatus::ACTUAL, {});
        const auto found_docs = server.FindTopDocuments("black cat"s);

        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].rating, 0);
    }

    //Проверяем расчет среднего рейтинга
    {
        const int AVERAGE_RAITING = 2;
        SearchServer server;

        server.AddDocument(0, content, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("black cat"s);

        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL_HINT(found_docs[0].rating, AVERAGE_RAITING, "The rating of the added document must be equal to the arithmetic mean of the ratings of the document"s);
    }
}

//Поиск документов, имеющих заданный статус.
void TestSearchDocumentByStatus() {
    SearchServer server;
    const vector<int> ratings = { 1, 2, 3 };
    const string content = "black cat in the city"s;

    //Нет документа с нужным статусом
    {
        const auto found_docs = server.FindTopDocuments("black cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 0);
    }

    //Добавим несколько документов с разными статусами
    server.AddDocument(0, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, content, DocumentStatus::REMOVED, ratings);
    server.AddDocument(2, content, DocumentStatus::BANNED, ratings);

    //Проверим, что находятся только документы с нужным статусом
    {
        const int BANNED_DOCUMENT_ID = 2;
        const auto found_docs = server.FindTopDocuments("black cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, BANNED_DOCUMENT_ID);
    }
}

//Корректное вычисление релевантности найденных документов.
void TestDocumentRelevanceComputing() {
    const double FIRST_DOCUMENT_RELEVANCE = 0.650672;
    const double SECOND_DOCUMENT_RELEVANCE = 0.274653;
    const double THIRD_DOCUMENT_RELEVANCE = 0.081093;

    SearchServer server;
    const vector<int> ratings = { 1, 2, 3 };

    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, ratings);

    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);

    ASSERT_EQUAL(found_docs.size(), 3);

    //Ожидаем, что первым будет документ с наибольшим рейтингом
    ASSERT(abs(found_docs[0].relevance - FIRST_DOCUMENT_RELEVANCE) < ELIPSON);
    ASSERT(abs(found_docs[1].relevance - SECOND_DOCUMENT_RELEVANCE) < ELIPSON);
    ASSERT(abs(found_docs[2].relevance - THIRD_DOCUMENT_RELEVANCE) < ELIPSON);
}

//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilterDocumentsByUserPredicate() {
    SearchServer server;
    const vector<int> ratings = { 1, 2, 3 };

    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, ratings);

    //Поищем без предиката
    {
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(found_docs.size(), 3);
    }

    //Поищем с предикатом
    {
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 1; });
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 1);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);

    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestDocumentMatching);
    RUN_TEST(TestDocumentsSortByRelevance);
    RUN_TEST(TestComputionDocumentRating);
    RUN_TEST(TestSearchDocumentByStatus);
    RUN_TEST(TestDocumentRelevanceComputing);
    RUN_TEST(TestFilterDocumentsByUserPredicate);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}