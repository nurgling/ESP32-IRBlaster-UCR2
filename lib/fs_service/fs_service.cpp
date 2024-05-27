// Copyright by Alex Koessler

// Provides service for spiffs handling.



#include <Arduino.h>
#include "fs_service.h"
#include "spiffs_fileinfo.h"

#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <string>

#include <esp_log.h>

static const char *TAG = "fsservice";

SPIFFSService::SPIFFSService(){
    if(!SPIFFS.begin(true)){
        ESP_LOGE(TAG, "An Error has occurred while mounting SPIFFS");
        SPIFFSStarted = false;
    } else {
        SPIFFSStarted = true;
    }
}

String SPIFFSService::path2filename(String path){
    int start = 0;
    int found = path.indexOf('/', start);

    while(found >= 0){
        start = found+1;
        found = path.indexOf('/', start);
    }
    if(start > 0){
        return(path.substring(start));
    } else {
        return path;
    }
}

String SPIFFSService::filename2path(String filename){
    String retval;
    if(!filename.startsWith("/")){
        retval = String("/") + filename;
    } else {
        retval = filename;
    }
    return retval;
}

String SPIFFSService::readFile(String filepath){
    if(!SPIFFSStarted){
        ESP_LOGE(TAG, "Error reading file '%s'. SPIFFS FS not mounted.", filepath);
        return "";
    }
    String filename = path2filename(filepath);

    std::map<String, SPIFFSFileInfo>::iterator it = index.find(filename);
    if (it != index.end()){
        //file found in index
        SPIFFSFileInfo finfo = it->second;
        if (finfo.getState() == FileMissing){
            //file in index, but missin in spiffs
            ESP_LOGE(TAG, "Blocked access to indexed file %s missing in SPIFFS", filepath);
            return String();
        } else {
            //file is indexed and found in SPIFFS. read it.
            return readFileInternal(filepath);
        }
    } else {
        //file not found in index
        ESP_LOGE(TAG, "Blocked access to non-indexed file %s", filepath);
        return String();
    }
}

String SPIFFSService::readFileInternal(String filepath){
    if(!SPIFFSStarted){
        ESP_LOGE(TAG, "Error reading file '%s'. SPIFFS FS not mounted.", filepath);
        return "";
    }

    String filecontent = "";
    File file = SPIFFS.open(filepath);
    if(!file){
        ESP_LOGE(TAG, "Failed to open file '%s'. Please update filesystem image to latest version!", filepath.c_str());
        return "";
    }
    ESP_LOGD(TAG, "Reading File %s. Size %d", filepath.c_str(), file.size());

    while(file.available()){
        filecontent += file.readString();
    }
    file.close();
    return filecontent;
}

void SPIFFSService::readManifest(){
    String manifestContent = readFileInternal("/spiffs_manifest.json");
    JsonDocument input;

    DeserializationError err = deserializeJson(input, manifestContent);
    if (err)
    {
        ESP_LOGE(TAG, "deserializeJson() of spiffs_manifest.json failed with code %s", err.f_str());
    }
    
    if (!input.isNull())
    {
        String version;

        if (input.containsKey("manifest_version"))
        {
            version = input["manifest_version"].as<String>();
            JsonObject files = input["files"]; 
            for(JsonPair kv : files){
                String filename = kv.key().c_str();
                String MD5Val = kv.value().as<String>();
                String filepath = filename2path(filename);

                if(SPIFFS.exists(filepath)){
                    SPIFFSFileInfo fsInfo = SPIFFSFileInfo(filepath, MD5Val);
                    index.insert(std::make_pair(filename, fsInfo));
                    ESP_LOGD(TAG, "Added file '%s' to SPIFFS index", filepath.c_str());
                } else {
                    SPIFFSFileInfo fsInfo = SPIFFSFileInfo(filepath, MD5Val, false);
                    index.insert(std::make_pair(filename, fsInfo));
                    ESP_LOGE(TAG, "Error adding file '%s' to SPIFFS index. File not found.", filepath.c_str());
                }
            }
        } else {
            ESP_LOGE(TAG, "SPIFFS Version not found in manifest. Please update filesystem image to latest version!");
        }     
    }
}

void SPIFFSService::init(){
    readManifest();
    std::map<String, SPIFFSFileInfo>::iterator it;
    for (it=index.begin(); it != index.end(); ++it){
        it->second.performMD5check();
    }
}

