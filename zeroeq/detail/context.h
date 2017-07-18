
/* Copyright (c) 2017, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

#pragma once

#include <memory>

namespace zeroeq
{
namespace detail
{
using ContextPtr = std::shared_ptr<void>;

/** @return the current ZeroMQ context. Users need to hold onto this context to
 *          extend its lifetime to all sockets created from the context.
 */
ContextPtr getContext();
}
}
