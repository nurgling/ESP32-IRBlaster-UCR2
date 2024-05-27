
import glob
import hashlib
import json
import os

DATAPATH = "data"
MANIFESTFILE="spiffs_manifest.json"


searchpath = os.path.join(DATAPATH, "*")
manifestpath = os.path.join(DATAPATH, MANIFESTFILE)
#print(searchpath)

filenames = glob.glob(searchpath)


with open(manifestpath, "r") as jsonFile:
    data = json.load(jsonFile)


if data["manifest_version"] != "0.1":
    print("Legacy manifest file found")
files = data["files"]
  
newfiles=dict()

for filepath in filenames:
    with open(filepath, 'rb') as inputfile:
        filename = os.path.basename(filepath)
        #we will not add the md5 of the file we are updating
        if filename != "spiffs_manifest.json":
            filecontent = inputfile.read()
            md5 = hashlib.md5(filecontent).hexdigest()
            newfiles[filename] = md5
                
#data.pop("files")
added=0
removed=0
changed=0
for f in newfiles:
    if f in files:
        if files[f] != newfiles[f]:
            changed += 1
            print("Updating MD5 of file {fname} from {oldmd5} -> {newmd5}".format(fname=f, oldmd5=files[f], newmd5=newfiles[f]))
    else:
        added += 1
        print("Adding MD5 of new file {fname} : {newmd5}".format(fname=f, newmd5=newfiles[f]))
for f in files:
    if f not in newfiles:
        removed += 1
        print("Removing MD5 of deleted file {fname}".format(fname=f))

if (added + removed + changed) == 0:
    print("No changes in manifest file required")
else:
    data["files"] = newfiles
    out = json.dumps(data, indent=4)

    with open(manifestpath, "w") as jsonFile:
        jsonFile.write(out)

