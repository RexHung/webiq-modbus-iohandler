//===-- Connect/variant_value_compare.h -------------------------*- C++ -*-===//
//
//                               WebIQ Connect
//
// This file is copyrighted by Smart HMI GmbH, Germany. Unauthorized
// distribution of this source file is prohibited. You may distribute
// binary code generated from this file by a compiler for private purposes.
// Use of this file in comercial applications is only permitted if used
// in conjunction with WebIQ.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains operators for comparing `variant_value`s with their
/// basic data types
///
/// \copyright
/// Smart HMI GmbH, 2013 - 2018
///
/// \date
/// 2017-09-22
///
/// \author
/// Tobias Kux <kux@smart-hmi.de>
///
//===----------------------------------------------------------------------===//
#ifndef SHMI_VARIANT_VALUE_COMPARE_H
#  define SHMI_VARIANT_VALUE_COMPARE_H
#  pragma once

#  include <type_traits>
#  include <limits>

#  include "variant_value.h"

namespace shmi {

bool operator<(const variant_value & lhs, bool rhs) {
	switch ( lhs.type() ) {
	case variant_value::kBool:
		return !lhs.getBool() && rhs;
	case variant_value::kInt:
		return !lhs.getInt() && rhs;
	case variant_value::kUInt:
		return !lhs.getUInt() && rhs;
	case variant_value::kDouble:
		return !lhs.getDouble() && rhs;
	default:
		return !lhs.toBool() && rhs;
	}
}

template<class _Ty>
typename std::enable_if<std::is_integral<_Ty>::value && std::is_unsigned<_Ty>::value, bool>::type
operator<(const variant_value & lhs, const _Ty & rhs) {
	switch ( lhs.type() ) {
	case variant_value::kBool:
		return !lhs.getBool() && !!rhs;
	case variant_value::kInt:
		return lhs.getInt() < 0 || static_cast<std::uint64_t>(lhs.getInt()) < rhs;
	case variant_value::kUInt:
		return lhs.getUInt() < rhs;
	case variant_value::kDouble:
		return lhs.getDouble() < static_cast<double>(rhs);
	default:
		return lhs.str().compare(std::to_string(rhs)) < 0;
	}
}

template<class _Ty>
typename std::enable_if<std::is_integral<_Ty>::value && std::is_signed<_Ty>::value, bool>::type
operator<(const variant_value & lhs, const _Ty & rhs) {
	switch ( lhs.type() ) {
	case variant_value::kBool:
		return !lhs.getBool() && !!rhs;
	case variant_value::kInt:
		return lhs.getInt() < rhs;
	case variant_value::kUInt:
		return rhs >= 0 && lhs.getUInt() < static_cast<std::uint64_t>(rhs);
	case variant_value::kDouble:
		return lhs.getDouble() < static_cast<double>(rhs);
	default:
		return lhs.str().compare(std::to_string(rhs)) < 0;
	}
}

template<class _Ty>
typename std::enable_if<std::is_floating_point<_Ty>::value, bool>::type
operator<(const variant_value & lhs, const _Ty & rhs) {
	switch ( lhs.type() ) {
	case variant_value::kBool:
		return !lhs.getBool() && !!rhs;
	case variant_value::kInt:
		return static_cast<double>(lhs.getInt()) < rhs;
	case variant_value::kUInt:
		return static_cast<double>(lhs.getUInt()) < rhs;
	case variant_value::kDouble:
		return lhs.getDouble() < rhs;
	default:
		return lhs.str().compare(std::to_string(rhs)) < 0;
	}
}

bool operator<(const variant_value & lhs, const std::string & rhs) {
	return lhs.str().compare(rhs) < 0;
}

bool operator<(const variant_value & lhs, const char * const rhs) {
	return lhs.str().compare(rhs) < 0;
}

bool operator<(const variant_value & lhs, const variant_value & rhs) {
	switch ( rhs.type() ) {
	case variant_value::kBool:
		return lhs < rhs.getBool();
	case variant_value::kInt:
		return lhs < rhs.getInt();
	case variant_value::kUInt:
		return lhs < rhs.getUInt();
	case variant_value::kDouble:
		return lhs < rhs.getDouble();
	default:
		return lhs < rhs.str();
	}
}

} // namespace shmi

#endif // SHMI_VARIANT_VALUE_COMPARE_H
