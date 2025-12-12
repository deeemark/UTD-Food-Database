/*Directly manages the storage file*/
#ifndef FILE_DISK_MANAGER_H
#define FILE_DISK_MANAGER_H

#include <string>
#include <fstream>
using namespace std;
//page size can be changed depending on how much
// data is being stored
static const int PAGE_SIZE = 16384;

class FileDiskManager {
private:
    std::string filename;
    std::fstream file;
    int nextPageId;
    //makes sure file is open and if it isnt create the file and reopen
    void EnsureOpen();

public:
    FileDiskManager(const std::string& filename);
    //read page into dst give pageId
    void ReadPage(int pageId, char* dst);
    //write a page into disk given pageId
    void WritePage(int pageId, const char* src);
    //create a new page and fill it with 0's
    int NewPageId();
    int GetNumPages() const { return nextPageId; }
};

#endif