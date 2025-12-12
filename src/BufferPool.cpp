#include "BufferPool.h"
#include <cstring>

BufferPool::BufferPool(int poolSize, FileDiskManager* dm)
{
    this->poolSize = poolSize;
    this->disk = dm;

    // Create frames dynamically and link them
    for (int i = 0; i < poolSize; i++)
    {
        auto* f = new PageFrame();
        f->data = new char[PAGE_SIZE];
        frameList.PushBack(f);
    }
}

BufferPool::~BufferPool()
{
    FlushAllPages();

    // Delete frames
    PageFrame* cur = frameList.Front();
    while (cur)
    {
        PageFrame* nxt = cur->next;
        delete[] cur->data;
        delete cur;
        cur = nxt;
    }
}

PageFrame* BufferPool::FetchPage(int pageId)
{
    fetches++;

    // BUFFER HIT
    auto it = pageTable.find(pageId);
    if (it != pageTable.end())
    {
        hits++;
        it->second->refCount++;
        return it->second;
    }

    misses++;

    // BUFFER MISS
    for (PageFrame* f = frameList.Front(); f != nullptr; f = f->next)
    {
        if (f->pageId == -1)
        {
            disk->ReadPage(pageId, f->data);
            f->pageId = pageId;
            f->refCount = 1;
            f->dirty = false;
            pageTable[pageId] = f;
            return f;
        }
    }

    // frame has to be evicted
    for (PageFrame* victim = frameList.Front(); victim != nullptr; victim = victim->next)
    {
        if (victim->refCount == 0)
        {
            evictions++;

            if (victim->dirty)
            {
                disk->WritePage(victim->pageId, victim->data);
                writes++;
            }

            pageTable.erase(victim->pageId);

            disk->ReadPage(pageId, victim->data);
            victim->pageId = pageId;
            victim->refCount = 1;
            victim->dirty = false;
            pageTable[pageId] = victim;

            return victim;
        }
    }

    std::cerr << "ERROR: No available frame for eviction!\n";
    return nullptr;
}

void BufferPool::UnpinPage(int pageId, bool dirty)
{
    auto it = pageTable.find(pageId);
    if (it == pageTable.end()) return;

    PageFrame* f = it->second;
    if (f->refCount > 0)
        f->refCount--;

    if (dirty)
        f->dirty = true;
}

PageFrame* BufferPool::NewPage(int& newPageId)
{
    newPageId = disk->NewPageId();
    PageFrame* f = FetchPage(newPageId);
    std::memset(f->data, 0, PAGE_SIZE);
    f->dirty = true;
    return f;
}

void BufferPool::WritePage(int pageId)
{
    auto it = pageTable.find(pageId);
    if (it == pageTable.end()) return;

    PageFrame* f = it->second;

    if (f->dirty)
    {
        disk->WritePage(pageId, f->data);
        writes++;
        f->dirty = false;
    }
}

void BufferPool::FlushAllPages()
{
    for (PageFrame* f = frameList.Front(); f != nullptr; f = f->next)
    {
        if (f->pageId != -1 && f->dirty)
        {
            disk->WritePage(f->pageId, f->data);
            writes++;
            f->dirty = false;
        }
    }
}
void BufferPool::ClearAllFrames()
{
    // Reset every frame
    for (PageFrame* f = frameList.Front(); f != nullptr; f = f->next)
    {
        f->refCount = 0;
        f->dirty = false;
        f->pageId = -1;
    }

    // Clear lookup table
    pageTable.clear();

    // Reset statistics (optional)
    fetches = hits = misses = evictions = writes = 0;
}
