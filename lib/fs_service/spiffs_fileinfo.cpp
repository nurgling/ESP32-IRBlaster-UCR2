// Copyright by Alex Koessler

// Provides service for spiffs file handling.

#include <Arduino.h>

#include "spiffs_fileinfo.h"

#include <SPIFFS.h>
#include <MD5Builder.h>

#include <esp_log.h>

static const char *TAG = "fileinfo";



SPIFFSFileInfo::SPIFFSFileInfo(String path, bool available){
    this->filepath=path;
    this->md5="";
    if(!available){
        this->state=FileMissing;
    } else {
        this->state=MD5Failed;
    }
}

SPIFFSFileInfo::SPIFFSFileInfo(String path, String md5, bool available){
    this->filepath=path;
    this->md5=md5;
    this->md5.toLowerCase();
    if(!available){
        this->state=FileMissing;
    } else {
        this->state=MD5Pending;
    }
}

bool SPIFFSFileInfo::performMD5check(bool forceRecheck){
    bool retval = false;
    if(this->state == FileMissing){
        ESP_LOGW(TAG, "Skipping MD5 check of missing file %s", this->filepath.c_str());
        //do not touch state if file was missing
        return false;
    }
    if(this->state == MD5Passed){
        retval = true;
    }
    if(this->state == MD5Failed){
        retval = false;
    }

    if((this->state == MD5Pending) || (forceRecheck == true)){
        ESP_LOGD(TAG, "performing MD5 check of file %s", this->filepath.c_str());

        File file = SPIFFS.open(this->filepath);
        if(!file){
            ESP_LOGE(TAG, "Failed to open file '%s'. Please update filesystem image to latest version!", this->filepath);
            return "";
        }
        String calcMD5 = String();
        if (file.seek(0, SeekSet)) {
            MD5Builder md5;
            md5.begin();
            md5.addStream(file, file.size());
            md5.calculate();
            calcMD5 = md5.toString();
            calcMD5.toLowerCase();
        } else {
            ESP_LOGE(TAG, "file seek failed");
        }

        file.close();  

        if(calcMD5 == this->md5){
            this->state=MD5Passed;
            retval = true;
            ESP_LOGD(TAG, "MD5 check of file '%s' passed.", this->filepath.c_str());
        } else {
            this->state=MD5Failed;
            retval = false;
            ESP_LOGW(TAG, "MD5 check of file '%s' failed.", this->filepath.c_str());
            ESP_LOGD(TAG, "MD5Manifest: %s MD5File: %s", this->md5.c_str(), calcMD5.c_str());
        }
    }
    return (retval);
}