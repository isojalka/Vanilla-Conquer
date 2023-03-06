#include <proto/dos.h>

#include <string.h>
#include <stdlib.h>

#include "paths.h"

std::string PathsClass::Argv_Path(const char* cmd_arg)
{
    return std::string("PROGDIR:");
}

std::string PathsClass::Concatenate_Paths(const char* path1, const char* path2)
{
    size_t len;

    if (!path2[0]) {
        return std::string(path1);
    } else if (!path1[0]) {
        return std::string(path2);
    } else {
        len = strlen(path1);
        if (path1[len - 1] == ':' || path1[len - 1] == '/') {
            return std::string(path1) + path2;
        } else {
            return std::string(path1) + "/" + path2;
        }
    }
}

bool PathsClass::Create_Directory(const char* dirname)
{
    bool ret = true;
    char* tempname;
    size_t namelen;
    BPTR lock;
    size_t i;

    if (dirname == nullptr) {
        return ret;
    }

    tempname = strdup(dirname);
    if (tempname) {
        namelen = strlen(tempname);
        for (i = 0; i <= namelen; i++) {
            if (tempname[i] == '/' || tempname[i] == 0) {
                tempname[i] = 0;

                if ((lock = CreateDir(tempname))) {
                    UnLock(lock);
                } else if (IoErr() != ERROR_OBJECT_EXISTS) {
                    ret = false;
                    break;
                }

                tempname[i] = '/';
            }
        }

        free(tempname);
    }

    return ret;
}

const char* PathsClass::Data_Path()
{
    if (DataPath.empty()) {
        DataPath = "PROGDIR:data/";
    }

    return DataPath.c_str();
}

std::string PathsClass::Get_Filename(const char* path)
{
    return FilePart(path);
}

bool PathsClass::Is_Absolute(const char* path)
{
    return path != nullptr && strchr(path, ':') != 0;
}

const char* PathsClass::Program_Path()
{
    if (ProgramPath.empty()) {
        ProgramPath = "PROGDIR:";
    }

    return ProgramPath.c_str();
}

const char* PathsClass::User_Path()
{
    if (UserPath.empty()) {
        UserPath = "PROGDIR:user/";
    }

    return UserPath.c_str();
}
