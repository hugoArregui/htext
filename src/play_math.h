#ifndef __PLAY_MATH

#include <cmath>
#include <climits>
#include "play_platform.h"

union v2
{
    struct
    {
        real32 x, y;
    };
    real32 e[2];
};

union v3
{
    struct
    {
        real32 x, y, z;
    };
    struct
    {
        real32 r, g, b;
    };
    struct
    {
        v2 xy;
        real32 ignored0_;
    };
    struct
    {
        real32 ignored1_;
        v2 yz;
    };
    real32 e[3];
};

union v4
{
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                real32 r, g, b;
            };
        };
        real32 a;
    };
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                real32 x, y, z;
            };
        };
        real32 w;
    };
    struct
    {
        v2 xy;
        real32 ignored0_;
        real32 ignored1_;
    };
    real32 e[4];
};


inline v2
V2i(uint32 x, uint32 y)
{
    v2 result = { (real32) x, (real32) y };

    return result;
}

inline v2
V2(real32 x, real32 y)
{
    v2 result;

    result.x = x;
    result.y = y;

    return result;
}

inline v3
V3(real32 x, real32 y, real32 z)
{
    // https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html
    return v3 {
      .x = x,
      .y = y,
      .z = z
    };
}

inline v3
V3(v2 xy, real32 z)
{
    v3 result;

    result.x = xy.x;
    result.y = xy.y;
    result.z = z;

    return result;
}

inline v4
V4(real32 x, real32 y, real32 z, real32 w)
{
    return v4 {
      .x = x,
      .y = y,
      .z = z,
      .w = w
    };
}

struct Rectangle2
{
    v2 min;
    v2 max;
};

struct Rectangle3
{
    v3 min;
    v3 max;
};

inline real32
signOf(real32 value)
{
    return value >= 0 ? 1.0f : -1.0f;
}

inline real32
square(real32 a)
{
    return a * a;
}

inline real32
clamp(real32 min, real32 value, real32 max)
{
    real32 result = value;

    if (result < min)
    {
        result = min;
    }
    else if (result > max)
    {
        result = max;
    }

    return result;
}

inline real32
clamp01(real32 value)
{
    real32 result = clamp(0.0f, value, 1.0f);
    return result;
}

inline real32
clamp01MapToRange(real32 min, real32 t, real32 max)
{
    real32 result = 0.0f;

    real32 range = max - min;

    if (range != 0)
    {
        result = clamp01((t - min) / range);
    }

    return result;
}

inline real32
lerp(real32 a, real32 t, real32 b)
{
    real32 result = (1.0f - t)*a + t*b;

    return result;
}

inline real32
safeRatioN(real32 numerator, real32 divisor, real32 n)
{
    real32 result = n;

    if(divisor != 0.0f)
    {
        result = numerator / divisor;
    }

    return result;
}

inline real32
safeRatio0(real32 numerator, real32 divisor)
{
    real32 result = safeRatioN(numerator, divisor, 0.0f);

    return result;
}

inline real32
safeRatio1(real32 numerator, real32 divisor)
{
    real32 result = safeRatioN(numerator, divisor, 1.0f);

    return result;
}


//
// NOTE(casey): v2 operations
//

inline v2
hadamard(v2 a, v2 b)
{
    v2 result = { a.x*b.x, a.y*b.y };

    return result;
}

inline real32
inner(v2 a, v2 b)
{
    real32 result = a.x*b.x + a.y*b.y;

    return result;
}

inline real32
lengthSq(v2 a)
{
    real32 result = inner(a, a);

    return result;
}

inline v2
operator*(real32 a, v2 b)
{
    v2 result;

    result.x = a*b.x;
    result.y = a*b.y;

    return result;
}

inline v2
operator*(v2 b, real32 a)
{
    v2 result = a*b;

    return result;
}

inline v2 &
operator*=(v2 &b, real32 a)
{
    b = a * b;

    return b;
}

inline v2
operator-(v2 a)
{
    v2 result;

    result.x = -a.x;
    result.y = -a.y;

    return result;
}

inline v2
operator+(v2 a, v2 b)
{
    v2 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

inline v2 &
operator+=(v2 &a, v2 b)
{
    a = a + b;

    return a;
}

inline v2
operator-(v2 a, v2 b)
{
    v2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline Rectangle2
rectMinMax(v2 min, v2 max)
{
    Rectangle2 result;
    result.min = min;
    result.max = max;

    return result;
}

inline Rectangle2
rectCenterHalfDim(v2 center, v2 halfDim)
{
    v2 min = center - halfDim;
    v2 max = center + halfDim;

    return rectMinMax(min, max);
}

inline Rectangle2
rectCenterDim(v2 center, v2 dim)
{
    Rectangle2 result = rectCenterHalfDim(center, 0.5f * dim);

    return result;
}

inline v2
getDim(Rectangle2 rect)
{
    v2 result = rect.max - rect.min;
    return result;
}

inline v2
clamp01(v2 value)
{
    v2 result;

    result.x = clamp01(value.x);
    result.y = clamp01(value.y);

    return result;
}

inline v2
getBarycentric(Rectangle2 a, v2 p)
{
    v2 result;

    result.x = safeRatio0(p.x - a.min.x, a.max.x - a.min.x);
    result.y = safeRatio0(p.y - a.min.y, a.max.y - a.min.y);

    return result;
}

inline real32
length(v2 a)
{
    real32 result = sqrt(lengthSq(a));
    return result;
}

inline v2
normalize(v2 v)
{
    real32 invLenght = 1.0f / length(v);
    v2 result = invLenght * v;
    return result;
}

//
// NOTE: v3 operations
//

inline v3
hadamard(v3 a, v3 b)
{
    v3 result = { a.x*b.x, a.y*b.y, a.z*b.z };

    return result;
}

inline real32
inner(v3 a, v3 b)
{
    real32 result = a.x*b.x + a.y*b.y + a.z*b.z;

    return result;
}

inline real32
lengthSq(v3 a)
{
    real32 result = inner(a, a);

    return result;
}

inline real32
length(v3 a)
{
    real32 result = sqrt(lengthSq(a));
    return result;
}

inline v3
operator*(real32 a, v3 b)
{
    v3 result;

    result.x = a*b.x;
    result.y = a*b.y;
    result.z = a*b.z;

    return result;
}

inline v3
operator*(v3 b, real32 a)
{
    v3 result = a*b;

    return result;
}

inline v3 &
operator*=(v3 &b, real32 a)
{
    b = a * b;

    return(b);
}

inline v3
operator-(v3 a)
{
    v3 result;

    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;

    return result;
}

inline v3
operator+(v3 a, v3 b)
{
    v3 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;

    return result;
}

inline v3 &
operator+=(v3 &a, v3 b)
{
    a = a + b;

    return(a);
}

inline v3
operator-(v3 a, v3 b)
{
    v3 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;

    return result;
}

//
// NOTE: Rectangle3
//

inline bool32
rectanglesIntersect(Rectangle3 a, Rectangle3 b)
{
    bool32 result = !((b.max.x <= a.min.x) ||
                      (b.min.x >= a.max.x) ||
                      (b.max.y <= a.min.y) ||
                      (b.min.y >= a.max.y) ||
                      (b.max.z <= a.min.z) ||
                      (b.min.z >= a.max.z));

    return result;
}

inline v3
getBarycentric(Rectangle3 a, v3 p)
{
    v3 result;

    result.x = safeRatio0(p.x - a.min.x, a.max.x - a.min.x);
    result.y = safeRatio0(p.y - a.min.y, a.max.y - a.min.y);
    result.z = safeRatio0(p.z - a.min.z, a.max.z - a.min.z);

    return result;
}

inline v3
getMinCorner(Rectangle3 rect)
{
    return rect.min;
}

inline v3
getMaxCorner(Rectangle3 rect)
{
    return rect.max;
}

inline v3
getCenter(Rectangle3 rect)
{
    v3 result = 0.5f*(rect.min + rect.max);

    return result;
}

inline Rectangle3
rectMinMax(v3 min, v3 max)
{
    Rectangle3 result;
    result.min = min;
    result.max = max;

    return result;
}

inline Rectangle3
rectCenterHalfDim(v3 center, v3 halfDim)
{
    v3 min = center - halfDim;
    v3 max = center + halfDim;

    return rectMinMax(min, max);
}

inline Rectangle3
rectCenterDim(v3 center, v3 dim)
{
    Rectangle3 result = rectCenterHalfDim(center, 0.5f * dim);

    return result;
}

inline bool32
isInRectangle(Rectangle3 rectangle, v3 test)
{
    bool32 result = ((test.x >= rectangle.min.x) &&
                     (test.y >= rectangle.min.y) &&
                     (test.z >= rectangle.min.z) &&
                     (test.x < rectangle.max.x) &&
                     (test.y < rectangle.max.y) &&
                     (test.z < rectangle.max.z));

    return result;
}

inline Rectangle3
addRadiusTo(Rectangle3 rect, v3 radius)
{
    Rectangle3 result;
    result.min = rect.min - radius;
    result.max = rect.max + radius;

    return result;
}

inline Rectangle3
offset(Rectangle3 rect, v3 offset)
{
    Rectangle3 result;
    result.min = rect.min + offset;
    result.max = rect.max + offset;

    return result;
}

inline v3
lerp(v3 a, real32 t, v3 b)
{
    v3 result = (1.0f - t)*a + t*b;

    return result;
}

inline v3
normalize(v3 v)
{
    real32 invLenght = 1.0f / length(v);
    v3 result = invLenght * v;
    return result;
}

inline v3
toV3(v2 v, real32 z)
{
    v3 result;
    result.x = v.x;
    result.y = v.y;
    result.z = z;
    return result;
}

//
// NOTE(casey): v4 operations
//

inline v4
hadamard(v4 a, v4 b)
{
    v4 result = { a.x*b.x, a.y*b.y, a.z*b.z , a.w*b.w};

    return result;
}

inline v4
operator*(real32 a, v4 b)
{
    v4 result;

    result.x = a*b.x;
    result.y = a*b.y;
    result.z = a*b.z;
    result.w = a*b.w;

    return result;
}

inline v4
operator*(v4 b, real32 a)
{
    v4 result = a*b;

    return result;
}

inline v4 &
operator*=(v4 &b, real32 a)
{
    b = a * b;

    return b;
}

inline v4
operator+(v4 a, v4 b)
{
    v4 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;

    return result;
}

inline v4 &
operator+=(v4 &a, v4 b)
{
    a = a + b;

    return(a);
}


inline v4
lerp(v4 a, real32 t, v4 b)
{
    v4 result = (1.0f - t)*a + t*b;

    return result;
}

inline v4
toV4(v3 v, real32 w)
{
    v4 result;
    result.xyz = v;
    result.w = w;
    return result;
}


//

struct Rectangle2i
{
    int32 xMin, yMin;
    int32 xMax, yMax;
};


inline Rectangle2i
intersect(Rectangle2i a, Rectangle2i b) {
    Rectangle2i r;
    r.xMin = (a.xMin < b.xMin) ? b.xMin : a.xMin;
    r.yMin = (a.yMin < b.yMin) ? b.yMin : a.yMin;
    r.xMax = (a.xMax > b.xMax) ? b.xMax : a.xMax;
    r.yMax = (a.yMax > b.yMax) ? b.yMax : a.yMax;
    return r;
}

inline Rectangle2i
rectUnion(Rectangle2i a, Rectangle2i b) {
    Rectangle2i r;
    r.xMin = (a.xMin < b.xMin) ? a.xMin : b.xMin;
    r.yMin = (a.yMin < b.yMin) ? a.yMin : b.yMin;
    r.xMax = (a.xMax > b.xMax) ? a.xMax : b.xMax;
    r.yMax = (a.yMax > b.yMax) ? a.yMax : b.yMax;
    return r;
}

inline int32
getClampedRectArea(Rectangle2i a)
{
    int32 width = a.xMax - a.xMin;
    int32 height = a.yMax - a.yMin;

    int32 result = 0;
    if (width > 0 && height > 0)
    {
        result = width * height;
    }

    return result;
}

inline bool32
hasArea(Rectangle2i a)
{
    bool32 result = a.xMin < a.xMax && a.yMin < a.yMax;
    return result;
}

inline Rectangle2i
invertedInfinityRectangle()
{
    Rectangle2i result;

    result.xMin = result.yMin = INT_MAX;
    result.xMax = result.yMax = -INT_MAX;

    return result;
}

static v4
sRGB255ToLinear1(v4 c)
{
    real32 inv255 = 1.0f / 255.0f;
    v4 result = V4(square(inv255 * c.r),
                   square(inv255 * c.g),
                   square(inv255 * c.b),
                   inv255 * c.a);


    return result;
}

inline real32
squareRoot(real32 r)
{
    real32 result = sqrtf(r);
    return result;
}

static v4
linear1ToSRGB255(v4 c)
{
    v4 result = V4(255.0f * squareRoot(c.r),
                   255.0f * squareRoot(c.g),
                   255.0f * squareRoot(c.b),
                   255.0f * c.a);

    return result;
}

inline uint32
roundReal32ToUInt32(real32 real32)
{
    uint32 result = (uint32)roundf(real32);
    return result;
}

#define __PLAY_MATH
#endif
