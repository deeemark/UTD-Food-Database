#include "csvLoader.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <iostream>
using namespace std;

static inline void trimInPlace(string& s) {
    // left trim
    size_t start = 0;
    while (start < s.size() && isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    // right trim
    std::size_t end = s.size();
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    if (start == 0 && end == s.size()) return;
    s = s.substr(start, end - start);
}

string normalizeName(string name) {
    // Collapse multiple spaces
    string out;
    bool lastWasSpace = false;
    for (char c : name) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!lastWasSpace) {
                out.push_back(' ');
                lastWasSpace = true;
            }
        }
        else {
            out.push_back(c);
            lastWasSpace = false;
        }
    }
    // 3) Trim leading/trailing spaces
    trimInPlace(out);
    return out;
}

bool parseCSVLine(const std::string& rawLine,
    std::string& name,
    int& protein,
    int& calories,
    double& cost)
{
    if (rawLine.empty()) return false;
    std::string line = rawLine;
    // Optional: ignore comment lines
    if (!line.empty() && line[0] == '#') return false;
    std::vector<string> cols;
    std::string current;
    bool inQuotes = false;

    for (char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
        }
        else if (ch == ',' && !inQuotes) {
            cols.push_back(current);
            current.clear();
        }
        else {
            current.push_back(ch);
        }
    }
    cols.push_back(current);

    if (cols.size() < 4) {
        return false;
    }
    // Column 0: name (normalized)
    name = normalizeName(cols[0]);
    // Column 1: protein
    trimInPlace(cols[1]);
    // Column 2: calories
    trimInPlace(cols[2]);
    // Column 3: cost
    trimInPlace(cols[3]);
    try {
        protein = stoi(cols[1]);
        calories = stoi(cols[2]);
        cost = stod(cols[3]);
    }
    catch (...) {
        return false;
    }
    return true;
}

size_t loadCSVIntoTree(const string& path, BPlusTreePaged& tree)
{
    ifstream in(path);
    if (!in.is_open()) {
        cerr << "ERROR: Could not open CSV file: " << path << "\n";
        return 0;
    }
    string line;
    size_t inserted = 0;
    // Skip header (first line)
    if (!std::getline(in, line)) {
        cerr << "ERROR: CSV file appears to be empty: " << path << "\n";
        return 0;
    }
    while (getline(in, line)) {
        string name;
        int protein{}, calories{};
        double cost{};
        if (!parseCSVLine(line, name, protein, calories, cost))
            continue;
        // Build an alphabetical key from the cleaned name
        int key = alphabeticalKey32(name);
        tree.insert(key, name, protein, calories, cost);
        inserted++;
    }
    cout << "Loaded CSV. Inserted " << inserted << " rows.\n";
    return inserted;
}
