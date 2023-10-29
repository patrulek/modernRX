#pragma once

/*
* Convenient macro for using __assume and assert interchangeably, depending on the build mode.
* Not a part of RandomX algorithm.
*/

// This is exception over rule for not using preprocessor and macros.
// Its for convenience of using assert() in DEBUG and [[assume]] in NDEBUG mode instead of picking only one of them.
// TODO: replace internal __assume with standardized [[assume]] when released. 
#ifdef NDEBUG
// Convenient macro for using __assume() in release mode.
#define ASSERTUME(x) __assume(x);
#else
#include <cassert>
// Convenient macro for using assert() in debug mode.
// If expression can be evaluated at compile time, do a compile-time check.
#define ASSERTUME(x) ([&]() constexpr {  \
    if (std::is_constant_evaluated()) {  \
        if(!(x)) throw -1;               \
    } else {                             \
        assert(x);                       \
    }                                    \
}());  
#endif
