// Adapted from terrarium-poly-octave bc7ddf35362cf4b0e9d46b77bc21e490f79f9ed1.
// Copyright (c) 2024 Steven Schulteis. MIT; see LICENSE.
#pragma once

#include <cstring>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace pitch { namespace terrarium {
// Original approximation with explicit tiny-input guard and portable bit copies.
inline float fastInvSqrt(float x) noexcept
{
    static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float)==sizeof(std::uint32_t));
    if(x<1e-30f)return 0; // Prevent reciprocal overflow/denormal arithmetic near zero.
    std::uint32_t bits;std::memcpy(&bits,&x,sizeof(bits));bits=0x5F1FFFF9-(bits>>1);
    float y;std::memcpy(&y,&bits,sizeof(y));
    return y*(0.703952253f*(2.38924456f-(x*y*y)));
}

inline float fastSqrt(float x)
{
    x=std::max(0.f,x); // Approximation error can put a half-angle radicand slightly below zero.
    return fastInvSqrt(x) * x;
}

}}
