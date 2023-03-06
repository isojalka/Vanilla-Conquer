// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection
#include <proto/dos.h>

#include "paths.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char* PathsClass::Program_Path()
{
    if (ProgramPath.empty()) {
        ProgramPath = "PROGDIR:";
    }

    return ProgramPath.c_str();
}

const char* PathsClass::Data_Path()
{
    if (DataPath.empty()) {
        DataPath = "PROGDIR:data/";
    }

    return DataPath.c_str();
}

const char* PathsClass::User_Path()
{
    if (UserPath.empty()) {
        UserPath = "PROGDIR:user/";
    }

    return UserPath.c_str();
}

bool PathsClass::Create_Directory(const char* dirname)
{
    bool ret = true;

    if (dirname == nullptr) {
        return ret;
    }

    std::string temp = dirname;
    size_t pos = 0;
    do {
        pos = temp.find_first_of("/", pos + 1);
        if (mkdir(temp.substr(0, pos).c_str(), 0700) != 0) {
            if (errno != EEXIST) {
                ret = false;
                break;
            }
        }
    } while (pos != std::string::npos);

    return ret;
}

bool PathsClass::Is_Absolute(const char* path)
{
    return path != nullptr && strchr(path, ':') != 0;
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

std::string PathsClass::Get_Filename(const char *path)
{
    return FilePart(path);
}

std::string PathsClass::Argv_Path(const char* cmd_arg)
{
    return std::string("PROGDIR:");
}
