#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstdio> 
#include "bPlusTree.h"
#include "csvLoader.h"
#include "tests.h"

using namespace std;
static void printMenu() {
    cout << "\n=====================================\n";
    cout << "            FOOD DB MENU             \n";
    cout << "=====================================\n";
    cout << " 1) Search by exact name\n";
    cout << " 2) Search by prefix\n";
    cout << " 3) Browse by first-letter range\n";
    cout << " 4) Show stats (total items)\n";
    cout << " 5) Print B+ tree (debug)\n";
    cout << " 6) Top N by calories/protein/ratios\n";
    cout << " 7) Add or update an item\n";
    cout << " 8) Remove an item (with confirm)\n";
    cout << " 0) Exit\n";
    cout << "-------------------------------------\n";
    cout << "Enter choice: ";
}

int main() {

    cout << "=== CSV Demo Program ===\n";
    cout << "Enter CSV filename (in same folder as exe): ";
    remove("tree_data.bin");
    string filename;
    getline(cin, filename);
    // Initialize fresh disk, buffer pool, and tree
    FileDiskManager dm("tree_data.bin");
    BufferPool bp(10, &dm);
    BPlusTreePaged tree(&bp, &dm);
    // Load CSV into the tree
    cout << "\nLoading CSV...\n";
    size_t count = loadCSVIntoTree(filename, tree);
    if (count == 0) {
        cout << "\nERROR: No items loaded from CSV!\n";
        cout << "Please check:\n";
        cout << "  1. File exists and path is correct\n";
        cout << "  2. CSV format is correct (name,protein,calories,cost)\n";
        cout << "  3. File has data rows (not just header)\n";
        return 1;
    }
    cout << "Successfully inserted " << count << " items.\n";
    // Run performance tests
    cout << "\nRunning performance tests...\n";
    TestElementAccessTime(tree);
    testBloomAllLeaves(tree, 2000);
    cout << "NodePage size = " << sizeof(NodePage) << "\n";
    cout << "PAGE_SIZE = " << PAGE_SIZE << "\n";
    cout << "Tree depth = " << tree.computeTreeDepth() << "\n";

    bool running = true;
    while (running) {
        printMenu();

        string choiceLine;
        getline(cin, choiceLine);
        if (choiceLine.empty()) continue;
        char choice = choiceLine[0];

        switch (choice) {
        case '1': {
            cout << "\n=== Exact Name Search ===\n";
            cout << "Enter full item name: ";
            string rawName;
            getline(cin, rawName);

            string cleanedName = normalizeName(rawName);
            int key = alphabeticalKey32(cleanedName);

            foodItem result{};
            if (tree.search(key, result)) {
                cout << "\nFOUND ITEM:\n";
                cout << " Name:     " << result.foodName << "\n";
                cout << " Calories: " << result.calorieAmt << "\n";
                cout << " Protein:  " << result.proteinAmt << "\n";
                cout << " Cost:     $" << result.cost << "\n";
            }
            else {
                cout << "\nItem not found.\n";
            }
            break;
        }

        case '2': {
            cout << "\n=== Prefix Search ===\n";
            cout << "Enter prefix: ";
            string prefix;
            getline(cin, prefix);

            auto results = tree.prefixSearch(prefix);
            if (results.empty()) {
                cout << "\nNo items found with that prefix.\n";
            }
            else {
                cout << "\nFound " << results.size()
                    << " item(s) with prefix \"" << prefix << "\":\n";
                size_t shown = 0;
                for (const auto& f : results) {
                    cout << " - " << f.foodName
                        << "  (P=" << f.proteinAmt
                        << ", Cals=" << f.calorieAmt
                        << ", $" << f.cost << ")\n";
                    if (++shown >= 50) {
                        cout << "   ... (showing first 50)\n";
                        break;
                    }
                }
            }
            break;
        }

        case '3': {
            cout << "\n=== Browse by First-Letter Range ===\n";
            cout << "Enter starting letter (e.g., A): ";
            string s1, s2;
            getline(cin, s1);
            if (s1.empty()) {
                cout << "Invalid input.\n";
                break;
            }
            cout << "Enter ending letter (e.g., Z): ";
            getline(cin, s2);
            if (s2.empty()) {
                cout << "Invalid input.\n";
                break;
            }

            char c1 = s1[0];
            char c2 = s2[0];

            auto bucket = tree.rangeSearchByChar(c1, c2);
            if (bucket.empty()) {
                cout << "\nNo items found in that letter range.\n";
            }
            else {
                cout << "\nItems with first letter between '"
                    << c1 << "' and '" << c2 << "':\n";
                int shown = 0;
                for (const auto& pair : bucket) {
                    const foodItem& f = pair.second;
                    cout << " - " << f.foodName
                        << "  (P=" << f.proteinAmt
                        << ", Cals=" << f.calorieAmt
                        << ", $" << f.cost << ")\n";
                    if (++shown >= 50) {
                        cout << "   ... (showing first 50)\n";
                        break;
                    }
                }
                cout << "Total items in range: " << bucket.size() << "\n";
            }
            break;
        }

        case '4': {
            cout << "\n=== Stats ===\n";
            auto allMap = tree.rangeSearch(0, numeric_limits<int>::max());
            cout << "Total items in tree: " << allMap.size() << "\n";

            if (!allMap.empty()) {
                cout << "First 10 items:\n";
                int shown = 0;
                for (const auto& pair : allMap) {
                    cout << " - " << pair.second.foodName << "\n";
                    if (++shown >= 10) break;
                }
            }
            break;
        }

        case '5': {
            cout << "\n=== B+ TREE STRUCTURE ===\n";
            tree.printTree();
            break;
        }

        case '6': {
            cout << "\n=== Top N by Calories / Protein / Ratios ===\n";
            cout << "Sort by:\n";
            cout << " 1) Calories (highest first)\n";
            cout << " 2) Protein  (highest first)\n";
            cout << " 3) Protein per calorie (P / C)\n";
            cout << " 4) Protein per dollar  (P / $)\n";
            cout << "Enter choice: ";
            string sortChoiceLine;
            getline(cin, sortChoiceLine);
            char sortChoice = sortChoiceLine.empty() ? '1' : sortChoiceLine[0];

            cout << "Enter N (how many items to show): ";
            string nLine;
            getline(cin, nLine);
            int N = 10;
            try {
                if (!nLine.empty())
                    N = stoi(nLine);
            }
            catch (...) {
                N = 10;
            }
            if (N <= 0) N = 10;

            auto allMap = tree.rangeSearch(0, numeric_limits<int>::max());
            if (allMap.empty()) {
                cout << "\nNo items in tree.\n";
                break;
            }

            vector<foodItem> items;
            items.reserve(allMap.size());
            for (const auto& pair : allMap) {
                items.push_back(pair.second);
            }

            if (sortChoice == '2') {
                sort(items.begin(), items.end(),
                    [](const foodItem& a, const foodItem& b) {
                        if (a.proteinAmt == b.proteinAmt)
                            return a.calorieAmt > b.calorieAmt;
                        return a.proteinAmt > b.proteinAmt;
                    });
            }
            else if (sortChoice == '3') {
                sort(items.begin(), items.end(),
                    [](const foodItem& a, const foodItem& b) {
                        double ra = (a.calorieAmt > 0)
                            ? static_cast<double>(a.proteinAmt) / a.calorieAmt
                            : 0.0;
                        double rb = (b.calorieAmt > 0)
                            ? static_cast<double>(b.proteinAmt) / b.calorieAmt
                            : 0.0;
                        if (ra == rb) return a.proteinAmt > b.proteinAmt;
                        return ra > rb;
                    });
            }
            else if (sortChoice == '4') {
                sort(items.begin(), items.end(),
                    [](const foodItem& a, const foodItem& b) {
                        double ra = (a.cost > 0.0)
                            ? static_cast<double>(a.proteinAmt) / a.cost
                            : 0.0;
                        double rb = (b.cost > 0.0)
                            ? static_cast<double>(b.proteinAmt) / b.cost
                            : 0.0;
                        if (ra == rb) return a.proteinAmt > b.proteinAmt;
                        return ra > rb;
                    });
            }
            else {
                sort(items.begin(), items.end(),
                    [](const foodItem& a, const foodItem& b) {
                        if (a.calorieAmt == b.calorieAmt)
                            return a.proteinAmt > b.proteinAmt;
                        return a.calorieAmt > b.calorieAmt;
                    });
            }

            if (N > static_cast<int>(items.size()))
                N = static_cast<int>(items.size());

            cout << "\nShowing top " << N << " item(s) by ";
            if (sortChoice == '2')      cout << "protein:\n";
            else if (sortChoice == '3') cout << "protein per calorie (P/C):\n";
            else if (sortChoice == '4') cout << "protein per dollar (P/$):\n";
            else                        cout << "calories:\n";

            for (int i = 0; i < N; ++i) {
                const auto& f = items[i];
                cout << " " << (i + 1) << ") " << f.foodName
                    << "  (Cals=" << f.calorieAmt
                    << ", P=" << f.proteinAmt
                    << ", $" << f.cost << ")";
                if (sortChoice == '3' && f.calorieAmt > 0) {
                    double ratio = static_cast<double>(f.proteinAmt) / f.calorieAmt;
                    cout << "  [P/C=" << ratio << "]";
                }
                if (sortChoice == '4' && f.cost > 0.0) {
                    double ratio = static_cast<double>(f.proteinAmt) / f.cost;
                    cout << "  [P/$=" << ratio << "]";
                }
                cout << "\n";
            }
            break;
        }

        case '7': {
            cout << "\n=== Add or Update Item ===\n";
            cout << "Enter item name: ";
            string rawName;
            getline(cin, rawName);

            string cleanedName = normalizeName(rawName);
            int key = alphabeticalKey32(cleanedName);

            foodItem existing{};
            bool exists = tree.search(key, existing);

            int protein = 0, calories = 0;
            double cost = 0.0;

            if (exists) {
                cout << "\nItem already exists:\n";
                cout << " Current Name:     " << existing.foodName << "\n";
                cout << " Current Calories: " << existing.calorieAmt << "\n";
                cout << " Current Protein:  " << existing.proteinAmt << "\n";
                cout << " Current Cost:     $" << existing.cost << "\n";

                cout << "\nDo you want to update this item? (Y/N): ";
                string yn;
                getline(cin, yn);
                if (yn.empty() || (yn[0] != 'y' && yn[0] != 'Y')) {
                    cout << "Update cancelled.\n";
                    break;
                }

                protein = existing.proteinAmt;
                calories = existing.calorieAmt;
                cost = existing.cost;

                cout << "\nPress Enter to keep the existing value.\n";

                cout << "New Protein (grams) [" << protein << "]: ";
                string pLine;
                getline(cin, pLine);
                try { if (!pLine.empty()) protein = stoi(pLine); }
                catch (...) {}

                cout << "New Calories [" << calories << "]: ";
                string cLine;
                getline(cin, cLine);
                try { if (!cLine.empty()) calories = stoi(cLine); }
                catch (...) {}

                cout << "New Cost ($) [" << cost << "]: ";
                string costLine;
                getline(cin, costLine);
                try { if (!costLine.empty()) cost = stod(costLine); }
                catch (...) {}
            }
            else {
                cout << "\nItem not found; creating new item.\n";

                cout << "Protein (grams): ";
                string pLine;
                getline(cin, pLine);
                try { if (!pLine.empty()) protein = stoi(pLine); }
                catch (...) {}

                cout << "Calories: ";
                string cLine;
                getline(cin, cLine);
                try { if (!cLine.empty()) calories = stoi(cLine); }
                catch (...) {}

                cout << "Cost ($): ";
                string costLine;
                getline(cin, costLine);
                try { if (!costLine.empty()) cost = stod(costLine); }
                catch (...) {}
            }

            tree.insert(key, cleanedName, protein, calories, cost);

            foodItem verify{};
            bool found = tree.search(key, verify);

            cout << "\nItem ";
            if (exists) cout << "updated";
            else        cout << "inserted";
            cout << " successfully!\n";

            if (found) {
                cout << "\n== Current Item Details ==\n";
                cout << " Name:     " << verify.foodName << "\n";
                cout << " Calories: " << verify.calorieAmt << "\n";
                cout << " Protein:  " << verify.proteinAmt << "\n";
                cout << " Cost:     $" << verify.cost << "\n";
            }

            auto allMap = tree.rangeSearch(0, numeric_limits<int>::max());
            cout << "\nTotal items now: " << allMap.size() << "\n";

            break;
        }

        case '8': {
            cout << "\n=== Remove Item ===\n";
            cout << "Enter full item name to remove: ";
            string rawName;
            getline(cin, rawName);

            string cleanedName = normalizeName(rawName);
            int key = alphabeticalKey32(cleanedName);

            foodItem existing{};
            bool found = tree.search(key, existing);

            if (!found) {
                cout << "\nItem not found — nothing to remove.\n";
                break;
            }

            cout << "\nItem found:\n";
            cout << " Name:     " << existing.foodName << "\n";
            cout << " Calories: " << existing.calorieAmt << "\n";
            cout << " Protein:  " << existing.proteinAmt << "\n";
            cout << " Cost:     $" << existing.cost << "\n";

            cout << "\nAre you sure you want to delete this item? (Y/N): ";
            string yn;
            getline(cin, yn);
            if (yn.empty() || (yn[0] != 'y' && yn[0] != 'Y')) {
                cout << "Delete cancelled.\n";
                break;
            }

            bool removed = tree.remove(key);

            if (removed) {
                cout << "\nItem \"" << cleanedName << "\" was removed successfully.\n";
            }
            else {
                cout << "\nRemove failed (item may have already been removed).\n";
            }

            auto allMap = tree.rangeSearch(0, numeric_limits<int>::max());
            cout << "\nTotal items now: " << allMap.size() << "\n";

            cout << "First 5 items now:\n";
            int shown = 0;
            for (const auto& p : allMap) {
                cout << " - " << p.second.foodName << "\n";
                if (++shown >= 5) break;
            }

            break;
        }

        case '0':
            running = false;
            break;

        default:
            cout << "Unknown option. Please choose 0–8.\n";
            break;
        }
    }

    cout << "\n=== Demo Complete ===\n";
    return 0;
}