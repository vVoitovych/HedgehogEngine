#include "api/HalfFloat.hpp"

#include <bit>

namespace ContentLoader
{
    uint16_t FloatToHalf(float value)
    {
        const uint32_t bits     = std::bit_cast<uint32_t>(value);
        const uint16_t sign     = static_cast<uint16_t>((bits >> 16) & 0x8000u);
        const uint32_t exponent = (bits >> 23) & 0xFFu;
        uint32_t       mantissa = bits & 0x7FFFFFu;

        if (exponent == 0xFFu) // infinity, or NaN kept quiet and non-zero
            return static_cast<uint16_t>(sign | 0x7C00u | (mantissa != 0 ? 0x200u : 0u));

        const int32_t halfExponent = static_cast<int32_t>(exponent) - 127 + 15;
        if (halfExponent >= 31)
            return static_cast<uint16_t>(sign | 0x7C00u);

        if (halfExponent <= 0)
        {
            // A subnormal half (or zero): the implicit bit made explicit, shifted into place, rounded.
            if (halfExponent < -10)
                return sign;
            mantissa |= 0x800000u;
            const uint32_t shift     = static_cast<uint32_t>(14 - halfExponent);
            uint32_t       result    = mantissa >> shift;
            const uint32_t remainder = mantissa & ((1u << shift) - 1u);
            const uint32_t halfway   = 1u << (shift - 1);
            if (remainder > halfway || (remainder == halfway && (result & 1u) != 0))
                ++result; // may carry into the smallest normal, which is the right answer
            return static_cast<uint16_t>(sign | result);
        }

        uint32_t       result    = (static_cast<uint32_t>(halfExponent) << 10) | (mantissa >> 13);
        const uint32_t remainder = mantissa & 0x1FFFu;
        if (remainder > 0x1000u || (remainder == 0x1000u && (result & 1u) != 0))
            ++result; // a carry into the exponent rounds up to the next power of two, or to infinity
        return static_cast<uint16_t>(sign | result);
    }

    float HalfToFloat(uint16_t half)
    {
        const uint32_t sign     = static_cast<uint32_t>(half & 0x8000u) << 16;
        const uint32_t exponent = (half >> 10) & 0x1Fu;
        uint32_t       mantissa = half & 0x3FFu;

        if (exponent == 0x1Fu)
            return std::bit_cast<float>(sign | 0x7F800000u | (mantissa << 13));
        if (exponent != 0)
            return std::bit_cast<float>(sign | ((exponent - 15 + 127) << 23) | (mantissa << 13));
        if (mantissa == 0)
            return std::bit_cast<float>(sign);

        // A subnormal half is a normal float: shift until the leading bit is the implicit one.
        uint32_t floatExponent = 127 - 15 + 1;
        while ((mantissa & 0x400u) == 0)
        {
            mantissa <<= 1;
            --floatExponent;
        }
        return std::bit_cast<float>(sign | (floatExponent << 23) | ((mantissa & 0x3FFu) << 13));
    }
}
