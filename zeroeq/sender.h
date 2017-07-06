
/* Copyright (c) 2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 */

#pragma once

namespace zeroeq
{
/** Interface for entities sending data. */
class Sender
{
public:
    virtual ~Sender() {}
    virtual void* getSocket() = 0; //!< @return the underlying ZMQ socket
};
}
