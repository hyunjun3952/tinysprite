#ifndef TINYSPRITE_UTIL_H
#define TINYSPRITE_UTIL_H

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

static inline int min_int(int a, int b)
{
    return a < b ? a : b;
}

static inline int max_int(int a, int b)
{
    return a > b ? a : b;
}

static inline int clamp_int(int value, int low, int high)
{
    return max_int(low, min_int(value, high));
}

#endif
