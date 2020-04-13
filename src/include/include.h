#ifndef INCLUDE_H
#define INCLUDE_H

#ifdef NDEBUG
    #undef NDEBUG
#endif

#if VDF_MODE==0
    #define NDEBUG
#endif

#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <fstream>

#ifndef _WIN32
#include <unistd.h>
typedef unsigned __int128 uint128;
typedef __int128 int128;
#define USED __attribute__((used))
#else
#include "uint128_t/uint128_t.h"
#define USED
#endif

#include <cassert>
#include <set>
#include <random>
#include <map>
#include <thread>
#include "generic.h"
#include <gmpxx.h>

using namespace std;
using namespace generic;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

#define todo

#endif // INCLUDE_H
