#pragma once
#include <cstddef>
#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator range_begin, Iterator range_end)
        :begin_(range_begin), end_(range_end), size_(distance(range_begin, range_end)) {
    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, IteratorRange<Iterator> iterator_range) {
    for (auto it = iterator_range.begin(); it != iterator_range.end(); ++it) {
        output << *it;
    }

    return output;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, size_t page_size){

        while (distance(range_begin, range_end) > page_size) {
            pages_.push_back(IteratorRange(range_begin, range_begin + page_size));
            advance(range_begin, page_size);
        }

        if (range_begin != range_end) {
            pages_.push_back(IteratorRange(range_begin, range_end));
        }

    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};


template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
