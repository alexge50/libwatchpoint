//
// Created by alex on 3/25/20.
//

#ifndef LIBWATCHPOINT_SRC_BYTE_LITERAL_H
#define LIBWATCHPOINT_SRC_BYTE_LITERAL_H

#include <cstddef>

constexpr std::byte operator ""_b(unsigned long long x)
{
    return static_cast<std::byte>(x);
}

#endif //LIBWATCHPOINT_SRC_BYTE_LITERAL_H
