#ifndef PAGE_FRAME_LIST_H
#define PAGE_FRAME_LIST_H

struct PageFrame
{
    int pageId;
    int refCount;
    bool dirty;
    char* data;
    PageFrame* prev;
    PageFrame* next;
    PageFrame() {
        pageId = -1;
        refCount = 0;
        dirty = false;
        data = nullptr;
        prev = next = nullptr;
    }
};

class FrameList
{
public:
    FrameList() : head(nullptr), tail(nullptr) {}
    PageFrame* Front() { return head; }
    PageFrame* Back() { return tail; }
    void PushBack(PageFrame* f)
    {
        f->prev = tail;
        f->next = nullptr;

        if (tail)
            tail->next = f;
        else
            head = f;

        tail = f;
    }
    void Remove(PageFrame* f)
    {
        if (f->prev)
            f->prev->next = f->next;
        else
            head = f->next;
        if (f->next)
            f->next->prev = f->prev;
        else
            tail = f->prev;
        f->prev = f->next = nullptr;
    }

private:
    PageFrame* head;
    PageFrame* tail;
};

#endif
