#ifndef BENCHMARKS_UTILS_H
#define BENCHMARKS_UTILS_H

#include <fstream>
#include <regex>
#include <string>
#include <algorithm>

std::vector<std::string> read_words_file(const std::string &filename) {
    std::vector<std::string> words;
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error(std::string("Failed to open file: ") + filename);
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::regex word_re("[A-Za-z]+(-[A-Za-z]+)?");
    auto begin = std::sregex_iterator(content.begin(), content.end(), word_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        words.emplace_back(it->str());
    }
    return words;
}

#endif // BENCHMARKS_UTILS_H