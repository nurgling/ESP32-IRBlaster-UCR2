// Copyright by Alex Koessler

// Provides service for spiffs handling.

#ifndef FS_SERVICE_H
#define FS_SERVICE_H


#include <Arduino.h>
#include <map>
#include "spiffs_fileinfo.h"


class SPIFFSService
{
public:

    static SPIFFSService& getInstance()
    {
        static SPIFFSService instance;
        return instance;
    }

    void init();
    String readFile(String filepath);


private:
    explicit SPIFFSService();
    virtual ~SPIFFSService() {}

    void readManifest();
    String readFileInternal(String filepath);
    String path2filename(String path);
    String filename2path(String filename);

    bool SPIFFSStarted=false;
    String manifestVersion;
    std::map<String, SPIFFSFileInfo> index;

};



#endif