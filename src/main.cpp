#include "FFDB.h"
#include <fcntl.h>
#include <fstream>
#include <regex>
#include <vector>

#include "BTree.h"

#define NODE_T 8

std::vector<std::string> read_words_file(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::string> words;

    // Read entire file into a string
    std::string content((std::istreambuf_iterator(file)),
                        std::istreambuf_iterator<char>());

    // Regex: one or more alphanumeric characters [a-zA-Z0-9]+
    std::regex regex(R"([a-zA-Z0-9]+)");

    auto start = std::sregex_iterator(content.begin(), content.end(), regex);
    auto end = std::sregex_iterator();

    for (auto it = start; it != end; ++it) words.push_back(it->str());

    return words;
}
// TODO: Figure out why SEGFAULT occurs. Seems that somewhere in BTree this becomes invalid when inserting the word felis.
int main() {
    remove("test.db");

    FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 200000);
    db.start_txn(DB_WRITE_TXN);

    auto tree = db.malloc<BTree::BTree<char, int, NODE_T>>();
    db.set_new_txn_root(tree.as_offset());

    auto words = read_words_file("words.txt");
    for (const auto& word : words) {

        if (word == "tempor") {
            tree->print_tree();
        }

        auto maybe_value = tree->get_value(word);
        if (!maybe_value) tree->insert(word, 1);
        else tree->update(word, ++maybe_value.value());
    }

    tree->print();

    db.commit_txn();

    remove("test.db");

    return 0;
}
