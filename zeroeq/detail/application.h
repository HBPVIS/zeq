/* Copyright (c) 2017, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

#pragma once

#include <zeroeq/log.h>

#include <climits>
#include <unistd.h>

#if __APPLE__
#include <dirent.h>
#include <mach-o/dyld.h>
#endif

namespace zeroeq
{
namespace detail
{
std::string getApplicationName()
{
// http://stackoverflow.com/questions/933850
#ifdef _MSC_VER
    char result[MAX_PATH];
    const std::string execPath(result,
                               GetModuleFileName(NULL, result, MAX_PATH));
#elif __APPLE__
    char result[PATH_MAX + 1];
    uint32_t size = sizeof(result);
    if (_NSGetExecutablePath(result, &size) != 0)
        return std::string();
    const std::string execPath(result);
#else
    char result[PATH_MAX];
    const ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count < 0)
    {
        // Not all UNIX have /proc/self/exe
        ZEROEQWARN << "Could not find absolute executable path" << std::endl;
        return std::string();
    }
    const std::string execPath(result, count > 0 ? count : 0);
#endif

#ifdef _MSC_VER
    const size_t lastSeparator = execPath.find_last_of('\\');
#else
    const size_t lastSeparator = execPath.find_last_of('/');
#endif
    if (lastSeparator == std::string::npos)
        return execPath;
    // lastSeparator + 1 may be at most equal to filename.size(), which is good
    return execPath.substr(lastSeparator + 1);
}
}
}
