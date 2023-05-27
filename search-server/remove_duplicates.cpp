#include "search_server.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicate_ids;

    set<set<string_view>> word_sets;

    for (const int document_id : search_server) {

        set<string_view> word_set;
        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            word_set.insert(word);
        }

        if (word_sets.count(word_set) > 0) {
            duplicate_ids.insert(document_id);
        }
        else {
            word_sets.insert(word_set);
        }
    }

    for (const int duplicate_id : duplicate_ids) {
        cout << "Found duplicate document id "s << duplicate_id << endl;
        search_server.RemoveDocument(duplicate_id);
    }
}