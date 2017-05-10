
/* Copyright (c) 2015, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

#include "sender.h"

namespace zeroeq
{
namespace detail
{
uint128_t& Sender::getUUID()
{
    static uint128_t identifier = servus::make_UUID();
    return identifier;
}
}
}
