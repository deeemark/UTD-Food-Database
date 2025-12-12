#include "bPlusTree.h"
#include <iostream>
#include <cstring>
#include <cctype>

// Load a node from the buffer pool
NodePage* BPlusTreePaged::loadNode(int pageId, PageFrame*& frame) const {
    frame = buffer->FetchPage(pageId);
    return reinterpret_cast<NodePage*>(frame->data);
}
/**********************************************************
Header Helpers
***********************************************************/
void BPlusTreePaged::writeHeader() {
    PageFrame* pf = buffer->FetchPage(0);
    BPTreeHeader hdr{ rootPageId, hasRoot ? 1 : 0 };
    memcpy(pf->data, &hdr, sizeof(hdr));
    buffer->UnpinPage(0, true);
}

void BPlusTreePaged::loadHeader() {
    PageFrame* pf = buffer->FetchPage(0);
    BPTreeHeader hdr{};
    memcpy(&hdr, pf->data, sizeof(hdr));
    buffer->UnpinPage(0, false);
    rootPageId = hdr.rootPageId;
    hasRoot = (hdr.hasRoot != 0);
}

// Bloom filter helper: rebuild from keys in a leaf node
void BPlusTreePaged::rebuildBloom(NodePage* node) {
    node->bloom.clear();
    if (!node->isLeaf) {
        return;
    }
    for (int i = 0; i < node->size; ++i) {
        node->bloom.add(node->keys[i]);
    }
}

/**********************************************************
Constructor
***********************************************************/
BPlusTreePaged::BPlusTreePaged(BufferPool* buffer, FileDiskManager* disk)
    : buffer(buffer), disk(disk), rootPageId(-1), hasRoot(false)
{
    if (disk->GetNumPages() == 0) {
        // Create EMPTY metadata page 0
        int       pid;
        PageFrame* pf = buffer->NewPage(pid);
        BPTreeHeader hdr{ -1, 0 };
        memcpy(pf->data, &hdr, sizeof(hdr));
        buffer->UnpinPage(pid, true);
    }
    else {
        // if there's more than 0 pages the header should exist
        loadHeader();
        return;
    }

    // Now load header from page 0
    loadHeader();
}


/**********************************************************
Node Creation Methods
***********************************************************/

int BPlusTreePaged::createLeafNode() {
    //create page int to store the page id from new page
    int pid;
    PageFrame* pf = buffer->NewPage(pid);
    //Zero out entire page to prevent stale data
    memset(pf->data, 0, PAGE_SIZE);
    NodePage* n = reinterpret_cast<NodePage*>(pf->data);
    n->isLeaf = true;
    n->size = 0;
    n->nextLeaf = -1;

    for (int i = 0; i < MAX_KEYS; ++i) {
        n->keys[i] = 0;
    }
    /*due to 0 being a valid page set children to -1 so you dont
    infinitely recurse through the tree*/
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        n->children[i] = -1;
    }
    buffer->UnpinPage(pid, true);
    return pid;
}

int BPlusTreePaged::createInternalNode() {
    //create page int to store the page id from new page
    int pid;
    PageFrame* pf = buffer->NewPage(pid);
    //Zero out entire page to prevent stale data
    memset(pf->data, 0, PAGE_SIZE);
    NodePage* n = reinterpret_cast<NodePage*>(pf->data);
    n->isLeaf = false;
    n->size = 0;
    n->nextLeaf = -1;
    for (int i = 0; i < MAX_KEYS; ++i) {
        n->keys[i] = 0;
    }
    /*due to 0 being a valid page set children to -1 so you dont
    infinitely recurse through the tree*/
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        n->children[i] = -1;
    }
    buffer->UnpinPage(pid, true);
    return pid;
}

/**********************************************************
Tree Management Helpers
***********************************************************/

BPlusTreePaged::InsertResult BPlusTreePaged::insertRecursive(int pageId, int key, const foodItem& item) {
    PageFrame* frame;
    NodePage* node = loadNode(pageId, frame);
    // Case 1: Leaf
    if (node->isLeaf) {
        // Duplicate keys are overwritten
        for (int i = 0; i < node->size; ++i) {
            if (node->keys[i] == key) {
                // Update existing entry
                node->items[i] = item;
                buffer->UnpinPage(pageId, true);
                return InsertResult(false);
            }
        }
        // Case 1.a: leaf is not full
        if (node->size < MAX_KEYS) {
            int i = node->size - 1;
            while (i >= 0 && key < node->keys[i]) {
                node->keys[i + 1] = node->keys[i];
                node->items[i + 1] = node->items[i];
                --i;
            }
            node->keys[i + 1] = key;
            node->items[i + 1] = item;
            node->size++;
            rebuildBloom(node);

            buffer->UnpinPage(pageId, true);
            return InsertResult(false);
        }
        // Case 1.b: Leaf is full and needs to be split
        /*first make temporary copies of page parameters
        with one extra space to store new key*/
        const int TOT = MAX_KEYS + 1;
        int tKeys[TOT];
        foodItem  tItems[TOT];
        for (int i = 0; i < MAX_KEYS; ++i) {
            tKeys[i] = node->keys[i];
            tItems[i] = node->items[i];
        }
        // Insert new key into temporary array
        int i = MAX_KEYS - 1;
        while (i >= 0 && key < tKeys[i]) {
            tKeys[i + 1] = tKeys[i];
            tItems[i + 1] = tItems[i];
            --i;
        }
        tKeys[i + 1] = key;
        tItems[i + 1] = item;
        // left gets first mid elements, right gets rest
        int mid = TOT / 2;
        node->size = mid;
        for (int j = 0; j < mid; ++j) {
            node->keys[j] = tKeys[j];
            node->items[j] = tItems[j];
        }
        rebuildBloom(node);
        int newLeaf = createLeafNode();
        PageFrame* nf;
        NodePage* nl = loadNode(newLeaf, nf);
        // store the rest of the temp array in the new leaf
        nl->size = TOT - mid;
        for (int j = 0; j < nl->size; ++j) {
            nl->keys[j] = tKeys[mid + j];
            nl->items[j] = tItems[mid + j];
        }
        nl->nextLeaf = node->nextLeaf;
        node->nextLeaf = newLeaf;
        rebuildBloom(nl);
        //keys track of left most key in new node for recursive propagation upwards
        int upKey = nl->keys[0];
        buffer->UnpinPage(pageId, true);
        buffer->UnpinPage(newLeaf, true);
        return InsertResult(true, upKey, newLeaf);
    }

    //Case 2: Internal
    int idx = 0;
    while (idx < node->size && key >= node->keys[idx]) {
        ++idx;
    }
    int childId = node->children[idx];
    buffer->UnpinPage(pageId, false);
    //Case 2.a: Split if the child has not been split no need to propate upKey
    /*VERY IMPORTANT: Recursively call on the children till you insert in the leaf
    and propagate the child's id/if it split/key that is getting brought upward information*/
    InsertResult cres = insertRecursive(childId, key, item);
    if (!cres.split) {
        return InsertResult(false);
    }
    PageFrame* frame2;
    NodePage* n2 = loadNode(pageId, frame2);
    //Case 2.b: Need to insert promoted key into this internal node
    //Case 2.b.i internal node is not full insert normally
    if (n2->size < MAX_KEYS) {
        int i = n2->size - 1;
        while (i >= 0 && cres.newKey < n2->keys[i]) {
            n2->keys[i + 1] = n2->keys[i];
            n2->children[i + 2] = n2->children[i + 1];
            --i;
        }
        n2->keys[i + 1] = cres.newKey;
        n2->children[i + 2] = cres.newRight;
        n2->size++;

        buffer->UnpinPage(pageId, true);
        return InsertResult(false);
    }
    //Case 2.b.ii internal node is full and has to be split
    const int TOTK = MAX_KEYS + 1;
    int       tKeys[TOTK];
    int       tChild[TOTK + 1];
    for (int i = 0; i < n2->size; ++i) {
        tKeys[i] = n2->keys[i];
    }
    for (int i = 0; i < n2->size + 1; ++i) {
        tChild[i] = n2->children[i];
    }
    int i = n2->size - 1;
    while (i >= 0 && cres.newKey < tKeys[i]) {
        tKeys[i + 1] = tKeys[i];
        tChild[i + 2] = tChild[i + 1];
        --i;
    }
    tKeys[i + 1] = cres.newKey;
    tChild[i + 2] = cres.newRight;
    int mid = TOTK / 2;
    int upKey = tKeys[mid];
    n2->size = mid;
    for (int j = 0; j < mid; ++j) {
        n2->keys[j] = tKeys[j];
        n2->children[j] = tChild[j];
    }
    n2->children[mid] = tChild[mid];
    int       newInt = createInternalNode();
    PageFrame* ff;
    NodePage* ni = loadNode(newInt, ff);
    ni->size = TOTK - mid - 1;
    for (int j = 0; j < ni->size; ++j) {
        ni->keys[j] = tKeys[mid + 1 + j];
        ni->children[j] = tChild[mid + 1 + j];
    }
    ni->children[ni->size] = tChild[TOTK];
    buffer->UnpinPage(pageId, true);
    buffer->UnpinPage(newInt, true);
    return InsertResult(true, upKey, newInt);
}

bool BPlusTreePaged::deleteRecursive(int pageId, int key, bool& removed) {
    PageFrame* pf;
    NodePage* node = loadNode(pageId, pf);
    const int  MIN_KEYS = ORDER;

    // Case 1: Leaf
    if (node->isLeaf) {
        int idx = -1;
        for (int i = 0; i < node->size; ++i) {
            if (node->keys[i] == key) { idx = i; break; }
        }
        if (idx == -1) {
            buffer->UnpinPage(pageId, false);
            removed = false;
            return false;
        }
        // Remove key/item
        for (int i = idx; i < node->size - 1; ++i) {
            node->keys[i] = node->keys[i + 1];
            node->items[i] = node->items[i + 1];
        }
        node->size--;
        rebuildBloom(node);
        removed = true;
        bool underflow = (pageId != rootPageId && node->size < MIN_KEYS);
        buffer->UnpinPage(pageId, true);
        return underflow;
    }

    // Case 2: Internal
    int idx = 0;
    while (idx < node->size && key >= node->keys[idx]) {
        ++idx;
    }
    int childId = node->children[idx];
    buffer->UnpinPage(pageId, false);
    /*
    Recurse till you get to the leaf and remove the node if possible:
    if it was removed and there was no underflow or the node was not 
    there return false
    */
    bool childUnderflow = deleteRecursive(childId, key, removed);
    if (!removed) {
        return false;
    }
    if (!childUnderflow) {
        return false;
    }
    //Case 2.a: search for child and remove it or return false if not found
    PageFrame* pf2;
    NodePage* parent = loadNode(pageId, pf2);
    int childIdx = -1;
    for (int i = 0; i <= parent->size; ++i) {
        if (parent->children[i] == childId) {
            childIdx = i; break;
        }
    }
    if (childIdx == -1) {
        buffer->UnpinPage(pageId, false);
        return false;
    }
    int leftIdx = (childIdx > 0) ? childIdx - 1 : -1;
    int rightIdx = (childIdx < parent->size) ? childIdx + 1 : -1;
    int leftId = (leftIdx != -1) ? parent->children[leftIdx] : -1;
    int rightId = (rightIdx != -1) ? parent->children[rightIdx] : -1;
    PageFrame* cf;
    NodePage* child = loadNode(childId, cf);
    const bool childIsLeaf = child->isLeaf;
    //Case 2.b: Handle underflow
    // Try to borrow from the left siblling 
    if (leftId != -1) {
        PageFrame* lf;
        NodePage* left = loadNode(leftId, lf);
        // if removing a key from the sibling doesn make it underflow
        if (left->size > MIN_KEYS) {
            // if it is a leaf adjust leaf only parameters
            if (childIsLeaf) {
                for (int i = child->size; i > 0; --i) {
                    child->keys[i] = child->keys[i - 1];
                    child->items[i] = child->items[i - 1];
                }
                child->keys[0] = left->keys[left->size - 1];
                child->items[0] = left->items[left->size - 1];
                child->size++;
                left->size--;
                parent->keys[leftIdx] = child->keys[0];
                rebuildBloom(child);
                rebuildBloom(left);
            }
            else { // if it is internal adjust internal only params
                for (int i = child->size; i > 0; --i) {
                    child->keys[i] = child->keys[i - 1];
                    child->children[i + 1] = child->children[i];
                }
                child->children[1] = child->children[0];
                child->keys[0] = parent->keys[leftIdx];
                child->children[0] = left->children[left->size];
                parent->keys[leftIdx] = left->keys[left->size - 1];
                child->size++;
                left->size--;
            }
            buffer->UnpinPage(leftId, true);
            buffer->UnpinPage(childId, true);
            buffer->UnpinPage(pageId, true);
            return false;
        }
        buffer->UnpinPage(leftId, false);
    }
    // Try to borrow from right sibling
    if (rightId != -1) {
        PageFrame* rf;
        NodePage* right = loadNode(rightId, rf);
        if (right->size > MIN_KEYS) {
            if (childIsLeaf) {
                child->keys[child->size] = right->keys[0];
                child->items[child->size] = right->items[0];
                child->size++;
                for (int i = 0; i < right->size - 1; ++i) {
                    right->keys[i] = right->keys[i + 1];
                    right->items[i] = right->items[i + 1];
                }
                right->size--;
                parent->keys[childIdx] = right->keys[0];
                rebuildBloom(child);
                rebuildBloom(right);
            }
            else {
                child->keys[child->size] = parent->keys[childIdx];
                child->children[child->size + 1] = right->children[0];
                child->size++;
                parent->keys[childIdx] = right->keys[0];
                for (int i = 0; i < right->size - 1; ++i) {
                    right->keys[i] = right->keys[i + 1];
                    right->children[i] = right->children[i + 1];
                }
                right->children[right->size - 1] = right->children[right->size];
                right->size--;
            }
            buffer->UnpinPage(rightId, true);
            buffer->UnpinPage(childId, true);
            buffer->UnpinPage(pageId, true);
            return false;
        }
        buffer->UnpinPage(rightId, false);
    }
    // merge the two nodes
    int mergeLeftIdx;
    int leftPid, rightPid;
    if (leftId != -1) {
        mergeLeftIdx = leftIdx;
        leftPid = leftId;
        rightPid = childId;
    }
    else {
        mergeLeftIdx = childIdx;
        leftPid = childId;
        rightPid = rightId;
    }
    PageFrame* lf;
    NodePage* left = loadNode(leftPid, lf);
    PageFrame* rf2;
    NodePage* right = loadNode(rightPid, rf2);
    //set leaf params
    if (childIsLeaf) {
        for (int i = 0; i < right->size; ++i) {
            left->keys[left->size + i] = right->keys[i];
            left->items[left->size + i] = right->items[i];
        }
        left->size += right->size;
        left->nextLeaf = right->nextLeaf;
        rebuildBloom(left);
    }
    else { //set internal params
        int oldL = left->size;
        left->keys[oldL] = parent->keys[mergeLeftIdx];
        left->children[oldL + 1] = right->children[0];

        for (int i = 0; i < right->size; ++i) {
            left->keys[oldL + 1 + i] = right->keys[i];
            left->children[oldL + 2 + i] = right->children[i + 1];
        }
        left->size = oldL + 1 + right->size;
    }
    // Mark the right page as deleted by zeroing it out
    right->isLeaf = childIsLeaf;
    right->size = 0;
    right->nextLeaf = -1;
    right->bloom.clear();
    buffer->UnpinPage(rightPid, true);  // Write the zeroed page
    for (int i = mergeLeftIdx; i < parent->size - 1; ++i) {
        parent->keys[i] = parent->keys[i + 1];
        parent->children[i + 1] = parent->children[i + 2];
    }
    parent->size--;
    bool underflowHere = (pageId != rootPageId && parent->size < MIN_KEYS);
    buffer->UnpinPage(leftPid, true);
    buffer->UnpinPage(pageId, true);
    buffer->UnpinPage(childId, false);
    return underflowHere;
}

/**********************************************************
Printing
***********************************************************/

void BPlusTreePaged::printNodeWithItems(int pageId, int depth) const {
    if (pageId < 0) return;
    PageFrame* pf;
    NodePage* node = loadNode(pageId, pf);
    // indentation by depth
    for (int i = 0; i < depth; i++)
        cout << "    ";
    // Header
    cout << (node->isLeaf ? "[Leaf] " : "[Int ] ")
        << "Page " << pageId
        << " | size=" << node->size;
    if (node->isLeaf)
        cout << " | nextLeaf=" << node->nextLeaf;
    cout << "\n";
    // Print keys
    for (int i = 0; i < depth; i++)
        cout << "    ";
    cout << "    Keys: ";
    for (int i = 0; i < node->size; i++)
        cout << node->keys[i] << (i + 1 < node->size ? ", " : "");
    cout << "\n";
    // If LEAF: print full foodItem records
    if (node->isLeaf) {
        for (int i = 0; i < node->size; i++) {
            for (int j = 0; j < depth; j++)
                cout << "    ";
            const foodItem& f = node->items[i];
            cout << "       • "
                << "key=" << node->keys[i]
                << " | name=\"" << f.foodName << "\""
                << " | protein=" << f.proteinAmt
                << " | calories=" << f.calorieAmt
                << " | cost=" << f.cost << "\n";
        }
    }
    // If INTERNAL: print child pointers
    if (!node->isLeaf) {
        for (int i = 0; i < depth; i++)
            cout << "    ";
        cout << "    Children: ";
        for (int i = 0; i <= node->size; i++) {
            cout << node->children[i];
            if (i < node->size) cout << ", ";
        }
        cout << "\n";
    }
    buffer->UnpinPage(pageId, false);
    // Recurse
    if (!node->isLeaf) {
        for (int i = 0; i <= node->size; i++) {
            int child = node->children[i];
            if (child != -1)
                printNodeWithItems(child, depth + 1);
        }
    }
}

void BPlusTreePaged::printTree() const {
    cout << "\n========== B+ TREE (WITH FOOD ITEMS) ==========\n";
    if (!hasRoot) {
        cout << "(empty tree)\n";
        return;
    }
    printNodeWithItems(rootPageId, 0);
    cout << "================================================\n";
}

/**********************************************************
Hashing Helpers
***********************************************************/

static inline uint16_t prefix16(const string& s)
{
    // Encode first 1–2 chars into 16 bits
    unsigned char c1 = 'A';
    unsigned char c2 = 0;  // 0 means "no second char"
    if (!s.empty()) {
        c1 = static_cast<unsigned char>(
            toupper(static_cast<unsigned char>(s[0])));
    }
    if (s.size() >= 2) {
        c2 = static_cast<unsigned char>(
            toupper(static_cast<unsigned char>(s[1])));
    }
    return static_cast<uint16_t>((uint16_t(c1) << 8) | uint16_t(c2));
}

// Encodes the key for storage
int alphabeticalKey32(const string& s)
{
    //upper 16 bits from characters
    uint16_t pref = prefix16(s);
    // Lower 16 bits from hash
    uint16_t h = static_cast<uint16_t>(hash<string>{}(s) & 0xFFFFu
        );
    uint32_t key = (uint32_t(pref) << 16) | uint32_t(h);
    return static_cast<int>(key);
}

/**********************************************************
Tree Management
***********************************************************/
void BPlusTreePaged::insert(int key,
    const string& name,
    int protein,
    int calories,
    double cost) {
    foodItem item(name, protein, calories, cost);
    //Case 1: insertion into a empty tree
    if (!hasRoot) {
        rootPageId = createLeafNode();
        hasRoot = true;
        writeHeader();
        PageFrame* pf;
        NodePage* r = loadNode(rootPageId, pf);
        // Check for duplicate in empty tree
        for (int i = 0; i < r->size; ++i) {
            if (r->keys[i] == key) {
                r->items[i] = item;
                buffer->UnpinPage(rootPageId, true);
                return;
            }
        }
        r->keys[0] = key;
        r->items[0] = item;
        r->size = 1;
        rebuildBloom(r);
        buffer->UnpinPage(rootPageId, true);
        return;
    }
    //Case 2: insertion into a Non-empty tree
    InsertResult res = insertRecursive(rootPageId, key, item);
    /*if split is true the split has propagated to the root
    so create a new root and add the old one as a child*/
    if (res.split) {
        int newRoot = createInternalNode();
        PageFrame* pf;
        NodePage* r = loadNode(newRoot, pf);
        r->isLeaf = false;
        r->size = 1;
        r->keys[0] = res.newKey;
        r->children[0] = rootPageId;
        r->children[1] = res.newRight;
        buffer->UnpinPage(newRoot, true);
        rootPageId = newRoot;
        hasRoot = true;
        writeHeader();
    }
    writeHeader();
}

bool BPlusTreePaged::remove(int key) {
    if (!hasRoot) return false;
    bool removed = false;
    deleteRecursive(rootPageId, key, removed);
    //if removal failed
    if (!removed) return false;
    PageFrame* pf;
    NodePage* root = loadNode(rootPageId, pf);
    //Case 1: if root is a internal node and empty
    if (!root->isLeaf && root->size == 0) {
        int newRootId = root->children[0];
        buffer->UnpinPage(rootPageId, false);
        rootPageId = newRootId;
        writeHeader();
        return true;
    }
    // Case 2:Root is leaf and empty
    if (root->isLeaf && root->size == 0) {
        buffer->UnpinPage(rootPageId, false);
        rootPageId = -1;
        hasRoot = false;
        writeHeader();  // Update header - tree is now empty
        return true;
    }
    buffer->UnpinPage(rootPageId, false);
    // Root didn't change, no need to update header
    return true;
}

int BPlusTreePaged::computeTreeDepth() const
{
    if (!hasRoot || rootPageId < 0)
        return 0;  // empty tree
    int depth = 0;
    int pid = rootPageId;
    while (pid != -1)
    {
        depth++;
        PageFrame* pf;
        NodePage* node = const_cast<BPlusTreePaged*>(this)->loadNodeForTest(pid, pf);
        if (!node) {
            const_cast<BPlusTreePaged*>(this)->unpinForTest(pid, false);
            break;
        }

        if (node->isLeaf) {
            const_cast<BPlusTreePaged*>(this)->unpinForTest(pid, false);
            break;  // reached leaf
        }

        // Follow the leftmost child
        int nextPid = node->children[0];
        const_cast<BPlusTreePaged*>(this)->unpinForTest(pid, false);

        pid = nextPid;
    }
    return depth;
}
 /********************************************************** 
 Searches 
 ***********************************************************/
int BPlusTreePaged::findLeafPage(int key) const {
    if (!hasRoot) return -1;
    int cur = rootPageId;
    while (true) {
        PageFrame* pf;
        NodePage* n = loadNode(cur, pf);
        if (n->isLeaf) {
            buffer->UnpinPage(cur, false);
            return cur;
        }
        int idx = 0;
        while (idx < n->size && key >= n->keys[idx]) {
            ++idx;
        }
        int nxt = n->children[idx];
        buffer->UnpinPage(cur, false);
        cur = nxt;
    }
}

bool BPlusTreePaged::search(int key, foodItem& out) const {
    int leafPage = findLeafPage(key);
    if (leafPage == -1) return false;
    PageFrame* pf;
    NodePage* leaf = loadNode(leafPage, pf);
    //check for bloom filter miss
    if (!leaf->bloom.possiblyContains(key)) {
        buffer->UnpinPage(leafPage, false);
        return false;
    }
    for (int i = 0; i < leaf->size; ++i) {
        if (leaf->keys[i] == key) {
            out = leaf->items[i];
            buffer->UnpinPage(leafPage, false);
            return true;
        }
    }
    buffer->UnpinPage(leafPage, false);
    return false;
}

unordered_map<int, foodItem> BPlusTreePaged::rangeSearch(int k1, int k2) const {
    unordered_map<int, foodItem> out;
    int leafPage = findLeafPage(k1);
    if (leafPage == -1) return out;
    int cur = leafPage;
    while (cur != -1) {
        PageFrame* pf;
        NodePage* leaf = loadNode(cur, pf);
        for (int i = 0; i < leaf->size; ++i) {
            int key = leaf->keys[i];
            if (key > k2) {
                buffer->UnpinPage(cur, false);
                return out;
            }
            if (key >= k1) {
                out[key] = leaf->items[i];
            }
        }
        int nxt = leaf->nextLeaf;
        buffer->UnpinPage(cur, false);
        cur = nxt;
    }
    return out;
}

unordered_map<int, foodItem> BPlusTreePaged::rangeSearchByChar(char c1, char c2) const
{
    if (c1 > c2)
        swap(c1, c2);
    unsigned char uc1 = static_cast<unsigned char>(toupper(static_cast<unsigned char>(c1)));
    unsigned char uc2 = static_cast<unsigned char>(toupper(static_cast<unsigned char>(c2)));
    // low bucket = C1 XX
    uint16_t lowPref16 =
        static_cast<uint16_t>((uint16_t(uc1) << 8) | 0x00u);
    // high bucket = C2 FF
    uint16_t highPref16 =
        static_cast<uint16_t>((uint16_t(uc2) << 8) | 0xFFu);
    int k1 = static_cast<int>((uint32_t(lowPref16) << 16));
    int k2 = static_cast<int>((uint32_t(highPref16) << 16) | 0x0000FFFFu);
    return rangeSearch(k1, k2);
}

vector<foodItem> BPlusTreePaged::prefixSearch(const string& prefix) const
{
    vector<foodItem> results;
    if (prefix.empty())
        return results;
    // Upper-case copy for bucket computations
    string up = prefix;
    for (char& ch : up) {
        ch = static_cast<char>(
            toupper(static_cast<unsigned char>(ch)));
    }
    uint16_t lowPref16, highPref16;
    if (up.size() >= 2) {
        // all names whose first two chars match
        unsigned char c1 = static_cast<unsigned char>(up[0]);
        unsigned char c2 = static_cast<unsigned char>(up[1]);

        lowPref16 = static_cast<uint16_t>((uint16_t(c1) << 8) | uint16_t(c2));
        highPref16 = lowPref16;
    }
    else {
        // Only 1 char: cover all second-char possibilities for that first letter
        unsigned char c1 = static_cast<unsigned char>(up[0]);
        lowPref16 = static_cast<uint16_t>((uint16_t(c1) << 8) | 0x00u);
        highPref16 = static_cast<uint16_t>((uint16_t(c1) << 8) | 0xFFu);
    }
    int lowKey = static_cast<int>((uint32_t(lowPref16) << 16));
    int highKey = static_cast<int>((uint32_t(highPref16) << 16) | 0x0000FFFFu);
    // Integer range search over that bucket
    auto bucket = rangeSearch(lowKey, highKey);
    // Exact prefix filter (case-insensitive)
    for (const auto& entry : bucket) {
        const foodItem& item = entry.second;
        if (matchesPrefix(prefix, item.foodName))
            results.push_back(item);
    }
    return results;
}

bool BPlusTreePaged::search_noBloom(int key, foodItem& out) const
{
    int leafPage = findLeafPage(key);
    if (leafPage == -1) return false;
    PageFrame* pf;
    NodePage* leaf = loadNode(leafPage, pf);
    for (int i = 0; i < leaf->size; ++i) {
        if (leaf->keys[i] == key) {
            out = leaf->items[i];
            buffer->UnpinPage(leafPage, false);
            return true;
        }
    }
    buffer->UnpinPage(leafPage, false);
    return false;
}

int BPlusTreePaged::getFirstLeafPageId() const
{
    if (!hasRoot)
        return -1;

    int pid = rootPageId;
    PageFrame* pf;
    int depth = 0;
    const int MAX_DEPTH = 20; // safety guard

    while (depth < MAX_DEPTH)
    {
        // ensure valid
        if (pid < 0) {
            cout << "ERROR: Invalid page ID " << pid << " at depth " << depth << "\n";
            return -1;
        }

        // NEVER treat page 0 as real node
        if (pid == 0) {
            cout << "ERROR: Attempted to traverse into header page 0\n";
            return -1;
        }

        NodePage* n = loadNode(pid, pf);
        if (!n) {
            cout << "ERROR: Unable to load page " << pid << "\n";
            buffer->UnpinPage(pid, false);
            return -1;
        }

        // If leaf ? return it
        if (n->isLeaf) {
            buffer->UnpinPage(pid, false);
            return pid;
        }

        // Find first valid child (skip -1 and skip header page 0)
        int child = -1;
        for (int i = 0; i <= n->size; i++) {
            int cid = n->children[i];

            // skip unused
            if (cid == -1)
                continue;

            // skip header page
            if (cid == 0)
                continue;

            child = cid;
            break;
        }

        buffer->UnpinPage(pid, false);

        if (child == -1) {
            cout << "ERROR: No valid child pointer found at page " << pid << "\n";
            cout << "       (all children were -1 or 0)\n";
            return -1;
        }

        pid = child;
        depth++;
    }

    cout << "ERROR: Max depth exceeded in getFirstLeafPageId\n";
    return -1;
}


bool BPlusTreePaged::matchesPrefix(const string& prefix, const char* foodName)
{
    if (!foodName || prefix.empty())
        return false;
    // Uppercase prefix
    string upPrefix = prefix;
    for (char& c : upPrefix)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    // Uppercase only the characters needed from foodName
    string upName;
    for (size_t i = 0; i < upPrefix.size() && foodName[i] != '\0'; i++)
    {
        upName.push_back(static_cast<char>(
            toupper(static_cast<unsigned char>(foodName[i]))
            ));
    }
    if (upName.size() < upPrefix.size())
        return false;
    return upName.compare(0, upPrefix.size(), upPrefix) == 0;
}

