#include "FloatMath.h"
#include <cmath>

float FloatMath::tan(float radians)
{
    return std::tan(radians);
}

float FloatMath::arcsin(float value)
{
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    return std::asin(value);
}

float FloatMath::arccos(float value)
{
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    return std::acos(value);
}

float FloatMath::arctan(float value)
{
    return std::atan(value);
}
