
/* Copyright (c) 2014-2017, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Stefan.Eilemann@epfl.ch
 */

#ifndef ZEROEQ_DETAIL_BROKER_H
#define ZEROEQ_DETAIL_BROKER_H

#include "constants.h"

#include <zeroeq/uri.h>

#include <cstring>
#include <sstream>

// getlogin()
#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Lmcons.h>
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace
{
inline std::string buildZmqURI(const std::string& schema, std::string host,
                               const uint16_t port)
{
    if (host.empty())
        host = "*";

    const std::string zmqURI(schema + "://" + host);
    if (port == 0) // zmq expects host:* instead of host:0
        return zmqURI + ":*";

    return zmqURI + ":" + std::to_string(int(port));
}

inline std::string buildZmqURI(const zeroeq::URI& uri)
{
    if (uri.getScheme() == DEFAULT_SCHEMA)
        return buildZmqURI(uri.getScheme(), uri.getHost(), uri.getPort());
    return std::to_string(uri);
}

inline std::string getUserName()
{
#ifdef _MSC_VER
    char user[UNLEN + 1];
    DWORD userLength = UNLEN + 1;
    GetUserName(user, &userLength);
#else
    const char* user = getlogin();
#endif
    return user ? user : UNKNOWN_USER;
}

inline std::string getDefaultSession()
{
    const char* session = getenv(ENV_SESSION.c_str());
    return session && strcmp(session, "") != 0 ? session : getUserName();
}
}

#endif
