// metal-cpp implementation translation unit.
//
// metal-cpp is header-only, but exactly one TU in the program must define these
// macros to emit the out-of-line selector tables and class references. Doing it
// anywhere else -- or twice -- produces duplicate symbols at link time.

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
