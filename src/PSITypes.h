// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Define global types.

#pragma once

#include <string>
#include <memory>
#include <type_traits>
#include <vector>

using std::unique_ptr;
using std::shared_ptr;
using std::move;
using std::make_shared;
using std::make_unique;
using std::vector;

// Needed together with enum classes
struct EnumClassHash {
    template <typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};
