#include "FileDiskManager.h"
#include <iostream>
#include <cstring>
#include <vector>
using namespace std;

void FileDiskManager::EnsureOpen() {
    if (!file.is_open()) { //if file currently isnt open
        file.open(filename, ios::in | ios::out | ios::binary);

        if (!file.is_open()) { //if the file isnt open still create a new file
            file.clear();
            file.open(filename, ios::out | ios::binary);
            file.close();
            // Reopen the file
            file.open(filename,
                ios::in | ios::out | ios::binary);
        }
    }
}

FileDiskManager::FileDiskManager(const string& filename)
    : filename(filename),
    nextPageId(0)
{
    EnsureOpen();
    //move the pointer toward to the end of file
    file.seekp(0, ios::end);

    streamoff endPos = file.tellp();
    //number of pages = file size/ page size
    //next page id = the number of pages due pages starting 0
    //allows for persistance access
    nextPageId = (endPos > 0) ? static_cast<int>(endPos / PAGE_SIZE) : 0;
}

void FileDiskManager::ReadPage(int pageId, char* dst) {
    EnsureOpen();
    // calculate staring offset by multipling it page size 
    // ie: page 2's starting offset is 2 * 4096
    streamoff offset = static_cast<streamoff>(pageId) * PAGE_SIZE;

    file.seekg(offset, ios::beg);
    file.read(dst, PAGE_SIZE);

    // If reading hits EOF, pad with zeroes
    if (file.gcount() != PAGE_SIZE) {
        streamsize n = file.gcount();
        memset(dst + n, 0, PAGE_SIZE - n);
    }
}

void FileDiskManager::WritePage(int pageId, const char* src) {
    EnsureOpen();

    streamoff offset = static_cast<streamoff>(pageId) * PAGE_SIZE;

    file.seekp(offset, ios::beg);
    file.write(src, PAGE_SIZE);
    file.flush();
}
int FileDiskManager::NewPageId() {
    EnsureOpen();

    int pid = nextPageId++;
    vector<char> zero(PAGE_SIZE, 0);

    WritePage(pid, zero.data());

    return pid;
}