#ifndef CSV_LOADER_H
#define CSV_LOADER_H

#include <string>
#include <cstddef>
#include "bPlusTree.h"

// Parses a CSV line into components:
//   rawLine  -> name, protein, calories, cost
// Returns false if the line is malformed.
bool parseCSVLine(const std::string& rawLine,
    std::string& name,
    int& protein,
    int& calories,
    double& cost);
// Normalize a food name the same way the CSV loader does:
// - strip "Variant ..."
// - remove parentheses and contents
// - trim spaces
std::string normalizeName(std::string name);

// Loads a CSV file into a B+ Tree.
// Returns the number of successfully inserted rows.
std::size_t loadCSVIntoTree(const std::string& path, BPlusTreePaged& tree);

#endif
