// Copyright by Alex Koessler

// Provides service for spiffs file handling.

#ifndef SPIFFS_FILEINFO_H
#define SPIFFS_FILEINFO_H


#include <Arduino.h>

enum FileStatus{
    FileMissing,
    MD5Failed,
    MD5Passed,
    MD5Pending,

};

class  SPIFFSFileInfo{
    public:
    SPIFFSFileInfo(String path, bool available = true);
    SPIFFSFileInfo(String path, String md5, bool available = true);

    bool performMD5check(bool forceRecheck = false);
    FileStatus getState() {return this->state;}

    private:
    String filepath;
    String md5;
    FileStatus state;
};

#endif
