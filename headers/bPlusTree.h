/*Page based B+ tree implementation that stores page information in bytes in given file.
Maximum children and keys are determined by order and certain parameters are only present
when the page is internal or external. The leaf nodes hold object data, pointers the new leaf
over, and a bloom filter*/
#ifndef BPLUSTREE_PAGED_H
#define BPLUSTREE_PAGED_H
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include "FileDiskManager.h"
#include "BufferPool.h"
#include "BloomFilter.h"
using namespace std;

// alphabetical key method to turn a string into a 32 bit int
int alphabeticalKey32(const std::string& s);

// food Item object to be stored in the page
struct foodItem {
    char  foodName[100]; // stored in a char array for easier serialization
    int   proteinAmt;
    int   calorieAmt;
    double cost;

    foodItem() {
        foodName[0] = '\0';
        proteinAmt = 0;
        calorieAmt = 0;
        cost = 0.0;
    }
    // full constructor
    foodItem(const std::string& name, int protein, int calories, double newCost) {
        std::size_t n = name.size();
        if (n >= sizeof(foodName)) n = sizeof(foodName) - 1;
        std::memcpy(foodName, name.data(), n);
        foodName[n] = '\0';

        proteinAmt = protein;
        calorieAmt = calories;
        cost = newCost;
    }
    // partial constructor
    foodItem(const std::string& name) {
        std::size_t n = name.size();
        if (n >= sizeof(foodName)) n = sizeof(foodName) - 1;
        std::memcpy(foodName, name.data(), n);
        foodName[n] = '\0';

        proteinAmt = 0;
        calorieAmt = 0;
        cost = 5.0;
    }
};

// B+ TREE ORDER for determining Keys/children
//The actual order value is max children. order is just a alias for t/min children
static const int ORDER = 15;
static const int MAX_KEYS = 2 * ORDER;
static const int MAX_CHILDREN = 2 * ORDER + 1;

// stores bp header information for persistence information
struct BPTreeHeader {
    int rootPageId;
    int hasRoot;
};

/* page holds information to distinguish leaf from internal
   and also Bloom filter data */
struct NodePage {
    bool isLeaf;
    int  size;
    int keys[MAX_KEYS];
    foodItem items[MAX_KEYS];
    int nextLeaf;
    int children[MAX_CHILDREN];
    BloomFilter bloom;   // embedded Bloom filter
};
static_assert(sizeof(NodePage) <= PAGE_SIZE,
    "ERROR: NodePage too large for PAGE_SIZE — increase PAGE_SIZE!");

/* B+ paged based implementation
   page/B+ tree node information is serialized into bytes and stored in a file */
class BPlusTreePaged {
public:
    BPlusTreePaged(BufferPool* buffer, FileDiskManager* disk);
    // B+ tree  management methods
    void insert(int key, const std::string& name, int protein, int calories, double cost);
    bool remove(int key);
    //returns tree depth
    int computeTreeDepth() const;
    // search methods
    //returns true if key is present
    bool search(int key, foodItem& out) const;
    //returns all food items by key range
    unordered_map<int, foodItem> rangeSearch(int k1, int k2) const;
    //returns all items by character range
    unordered_map<int, foodItem> rangeSearchByChar(char c1, char c2) const;
    //search for all items with given prefix
    vector<foodItem> prefixSearch(const std::string& prefix) const;
    bool search_noBloom(int key, foodItem& out) const;
    int findLeafPage(int key) const;
    int getFirstLeafPageId() const;

    //display methods
    void printTree() const;
    //  testing methods
    NodePage* loadNodeForTest(int pageId, PageFrame*& frame) const {
        return loadNode(pageId, frame);
    }
    void unpinForTest(int pageId, bool isDirty) const {
        buffer->UnpinPage(pageId, isDirty);
    }
    int getRootPageId() const {
        return rootPageId;
    }
private:
    // page access/management
    BufferPool* buffer;
    FileDiskManager* disk;
    // root information
    int  rootPageId;
    bool hasRoot;
    // header management
    void writeHeader(); // creates root page and adds header to root
    void loadHeader();  // loads existing header information for persistence
    //holds information for recursive operations
    struct InsertResult {
        bool split;
        int  newKey;
        int  newRight;
        InsertResult(bool s = false, int k = 0, int r = -1)
            : split(s), newKey(k), newRight(r) {
        }
    };
    /* given a pageId return the Page Frame/Node Page from the file/buffer and
       cast that data back to the node page */
    NodePage* loadNode(int pageId, PageFrame*& frame) const;
    // create leaf node with leaf only parameters
    int createLeafNode();
    //create leaf node with internal only parameters
    int createInternalNode();
    // Tree management Helpers
    InsertResult insertRecursive(int pageId, int key, const foodItem& item);
    bool deleteRecursive(int pageId, int key, bool& removed);
    // Bloom filter helper (rebuild from node->keys[])
    void rebuildBloom(NodePage* node);
    // print helper
    void printNodeWithItems(int pageId, int depth) const;
    // Helper for prefix search
    static bool matchesPrefix(const std::string& prefix, const char* foodName);

};

#endif
