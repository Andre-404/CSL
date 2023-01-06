#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

using std::string;
using std::vector;

typedef unsigned int uInt;
typedef unsigned long long uInt64;
typedef unsigned short uInt16;
typedef unsigned char byte;

constexpr double pi = 3.14159265358979323846;

//Using epsilon value because of floating point precision
#define FLOAT_EQ(x,v) (fabs(x - v) <= DBL_EPSILON)
#define IS_INT(num) (FLOAT_EQ(std::floor(num), num))

#define AST_DEBUG
#define COMPILER_DEBUG
//#define COMPILER_USE_LONG



