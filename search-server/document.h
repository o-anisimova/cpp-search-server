#pragma once
#include <iostream>

struct Document {
    Document();

    Document(int id, double relevance, int rating);

    int id;
    double relevance;
    int rating;
};

std::ostream& operator<<(std::ostream& output, Document document);

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};