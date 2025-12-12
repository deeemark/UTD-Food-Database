#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <vector>
#include "BufferPool.h"
#include "bPlusTree.h"
#include "tests.h"
using namespace std;
using namespace std::chrono;

// Average Element Access Time across leaf elements
void TestElementAccessTime(BPlusTreePaged& tree)
{
    using namespace std::chrono;
    cout << "\nAverage Element Access Time (Real Tree) ===\n";
    //collect all keys
    std::vector<int> allKeys;
    allKeys.reserve(20000);
    int pageId = tree.getFirstLeafPageId();
    if (pageId <= 0) {
        std::cout << "ERROR: Invalid first leaf page\n";
        return;
    }
    vector<int> visited;
    visited.reserve(2000);
    int leafCount = 0;
    while (pageId != -1)
    {
        if (std::find(visited.begin(), visited.end(), pageId) != visited.end())
            break;
        visited.push_back(pageId);

        PageFrame* pf;
        NodePage* leaf = tree.loadNodeForTest(pageId, pf);
        if (!leaf || !leaf->isLeaf) {
            tree.unpinForTest(pageId, false);
            break;
        }
        leafCount++;
        for (int i = 0; i < leaf->size; i++)
            allKeys.push_back(leaf->keys[i]);
        int next = leaf->nextLeaf;
        tree.unpinForTest(pageId, false);
        pageId = next;
    }
    if (allKeys.empty()) {
        std::cout << "ERROR: No keys found.\n";
        return;
    }
    sort(allKeys.begin(), allKeys.end());
    allKeys.erase(std::unique(allKeys.begin(), allKeys.end()), allKeys.end());
    size_t N = allKeys.size();
    const int TRIALS = 2000;
    //Time randomized access to measure average
    mt19937_64 rng(12345);
    foodItem out{};
    long long total = 0;
    for (int i = 0; i < TRIALS; i++)
    {
        int k = allKeys[rng() % N];
        auto t1 = high_resolution_clock::now();
        tree.search(k, out);
        auto t2 = high_resolution_clock::now();
        total += duration_cast<nanoseconds>(t2 - t1).count();
    }
    double avg = double(total) / TRIALS;
    cout << "Average Access Time: " << avg << " ns\n";
    cout << "=============================================\n";
}
// Tests bloom vs leaf scan on every leaf
void testBloomAllLeaves(BPlusTreePaged& tree, int trials)
{
    cout << "\nSAFE Bloom vs Leaf-Scan Test\n";
    long long totalBloom = 0;
    long long totalScan = 0;
    int leafCount = 0;
    // Start at first leaf element
    int pid = tree.getFirstLeafPageId();
    if (pid < 0) {
        cout << "ERROR: No leaf nodes found.\n";
        return;
    }
    vector<int> visited;
    visited.reserve(2000);
    while (pid != -1)
    {
        visited.push_back(pid);
        PageFrame* pf;
        NodePage* leaf = tree.loadNodeForTest(pid, pf);
        if (!leaf || !leaf->isLeaf) {
            tree.unpinForTest(pid, false);
            cout << "ERROR: Non-leaf page encountered in leaf chain.\n";
            break;
        }
        leafCount++;
        // Prepare miss keys
        vector<int> missKeys;
        missKeys.reserve(trials);
        for (int i = 0; i < trials; i++)
            missKeys.push_back(0x60000000 ^ (pid * 1315423911) ^ i);
        long long bloomThis = 0;
        long long scanThis = 0;
        // Bloom test
        for (int key : missKeys) {
            auto t1 = high_resolution_clock::now();
            leaf->bloom.possiblyContains(key);
            auto t2 = high_resolution_clock::now();
            bloomThis += duration_cast<nanoseconds>(t2 - t1).count();
        }
        // Full leaf scan
        for (int key : missKeys) {
            auto t1 = high_resolution_clock::now();
            for (int i = 0; i < leaf->size; i++)
                if (leaf->keys[i] == key)
                    break;
            auto t2 = high_resolution_clock::now();
            scanThis += duration_cast<nanoseconds>(t2 - t1).count();
        }
        tree.unpinForTest(pid, false);
        totalBloom += bloomThis;
        totalScan += scanThis;
        pid = leaf->nextLeaf;   // walk leaf chain
    }
    if (leafCount == 0) {
        cout << "ERROR: No leaf nodes scanned.\n";
        return;
    }
    cout << "\nLEAF RESULTS over " << leafCount << " leaves ---\n";
    cout << "Bloom MISS avg:     " << (totalBloom / double(leafCount * trials)) << " ns\n";
    cout << "Leaf scan MISS avg: " << (totalScan / double(leafCount * trials)) << " ns\n";
    cout << "Speedup:            "
        << (totalScan / double(totalBloom)) << "x\n";
    cout << "============================================================\n\n";
}
