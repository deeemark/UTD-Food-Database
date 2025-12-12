/* Buffer that holds recent pages that have been accessed or created
operates on FIFO and is implemented using a doubly linked list
*/
#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <iostream>
#include <unordered_map>
#include <string>
#include "FileDiskManager.h"
#include "pageFrameList.h"

using namespace std;

class BufferPool
{
public:
    //constructor
    BufferPool(int poolSize, FileDiskManager* disk);
    //destructor
    ~BufferPool();
    // Load page into memory or return existing one
    PageFrame* FetchPage(int pageId);
    /* frame is no longer being currently used
       Decrement pin count & mark dirty if needed */
    void UnpinPage(int pageId, bool dirty);
    // Create brand new page
    PageFrame* NewPage(int& newPageId);
    // Write page back manually
    void WritePage(int pageId);
    // Write back all dirty pages
    void FlushAllPages();
    FrameList* GetFrameList() { return &frameList; }
    // Buffer statistics
    long fetches = 0;
    long hits = 0;
    long misses = 0;
    long evictions = 0;
    long writes = 0; // number of disk writes (dirty flushes)
    void PrintStats(const std::string& label)
    {
        cout << "---- Buffer Stats (" << label << ") ----\n";
        cout << "Fetches:   " << fetches << "\n";
        cout << "Hits:      " << hits << "\n";
        cout << "Misses:    " << misses << "\n";
        cout << "Evictions: " << evictions << "\n";
        cout << "Writes:    " << writes << "\n";
        cout << "----------------------------------------\n";
    }

private:
public:
    void ClearAllFrames();
    int poolSize;
    FileDiskManager* disk;
    //doubly linked list frame buffer implementation
    FrameList frameList;
    // Maps pageId to frame pointer
    unordered_map<int, PageFrame*> pageTable;
};

#endif
