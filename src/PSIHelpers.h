// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Global helper utilities.

#pragma once

#include <array>

#define STACK_PUSH(x) (x.push(x.top()))

template<class Type>
std::unique_ptr<Type> copy_unique(const std::unique_ptr<Type>& source) {
	return source ? std::make_unique<Type>(*source) : nullptr;
}

template <typename T, size_t a, size_t b>
std::array<T, a + b> concat(std::array<T, a> const& x, std::array<T, b> const& y) {
	std::array<T, a + b> z;
	std::copy(x.begin(), x.end(), z.begin());
	std::copy(y.begin(), y.end(), z.begin() + x.size());

	return z;
}

template <typename E>
constexpr auto to_underlying(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}
