//===-- variant_value_helpers.h ---------------------------------*- C++ -*-===//
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
/// This file contains helpers and checked casting functions for
/// `variant_value`s.
///
/// \copyright
/// Smart HMI GmbH, 2013 - 2018
///
/// \date
/// 2017-11-23
///
/// \author
/// Tobias Kux <kux@smart-hmi.de>
///
//===----------------------------------------------------------------------===//
#ifndef SHMI_VARIANT_VALUE_HELPERS_H
#  define SHMI_VARIANT_VALUE_HELPERS_H
#  pragma once

#  include <limits>

#  include "shmi_cxx11_compat.h"
#  include "variant_value.h"

namespace shmi {

template<typename _Ty>
struct variant_value_type_id {
};

#define SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(type, variant_value_type) \
template<> \
struct variant_value_type_id<type> { \
	static SHMI_CONSTEXPR_OR_CONST variant_value::value_type value = (variant_value_type); \
};

SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(std::nullptr_t, variant_value::kUndefined);

SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(bool, variant_value::kBool);

SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(char, variant_value::kInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(short, variant_value::kInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(int, variant_value::kInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(long, variant_value::kInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(long long, variant_value::kInt);

SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(unsigned char, variant_value::kUInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(unsigned short, variant_value::kUInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(unsigned int, variant_value::kUInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(unsigned long, variant_value::kUInt);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(unsigned long long, variant_value::kUInt);

SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(float, variant_value::kDouble);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(double, variant_value::kDouble);

SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(char *, variant_value::kString);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(const char *, variant_value::kString);
SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE(std::string, variant_value::kString);

#  define SHMI_DEFINE_VARIANT_VALUE_INT_CAST_UNCHECKED(type) \
template<> \
inline type variant_value_cast<type>(const variant_value & value) { \
	return static_cast<type>(value.toInt()); \
}
#  define SHMI_DEFINE_VARIANT_VALUE_UINT_CAST_UNCHECKED(type) \
template<> \
inline type variant_value_cast<type>(const variant_value & value) { \
	return static_cast<type>(value.toUInt()); \
}
#  define SHMI_DEFINE_VARIANT_VALUE_FLT_CAST_UNCHECKED(type) \
template<> \
inline type variant_value_cast<type>(const variant_value & value) { \
	return static_cast<type>(value.toDouble()); \
}
#  define SHMI_DEFINE_VARIANT_VALUE_STR_CAST_UNCHECKED() \
template<> \
inline std::string variant_value_cast<std::string>(const variant_value & value) { \
	return value.str(); \
}

#  define SHMI_DEFINE_VARIANT_VALUE_INT_CAST_CHECKED(type) \
template<> \
inline type variant_value_cast<type>(const variant_value & value) { \
	long long val = value.toInt(); \
	if ( val < std::numeric_limits<type>::min() || std::numeric_limits<type>::max() < val ) { \
		throw std::domain_error("Value does not fit into target type"); \
	} \
	return static_cast<type>(val); \
}
#  define SHMI_DEFINE_VARIANT_VALUE_UINT_CAST_CHECKED(type) \
template<> \
inline type variant_value_cast<type>(const variant_value & value) { \
	unsigned long long val = value.toUInt(); \
	if ( std::numeric_limits<type>::max() < val ) { \
		throw std::domain_error("Value does not fit into target type"); \
	} \
	return static_cast<type>(val); \
}
#  define SHMI_DEFINE_VARIANT_VALUE_FLT_CAST_CHECKED(type) \
template<> \
inline type variant_value_cast<type>(const variant_value & value) { \
	double val = value.toDouble(); \
	if ( val < -std::numeric_limits<type>::max() || std::numeric_limits<type>::max() < val ) { \
		throw std::domain_error("Value does not fit into target type"); \
	} \
	return static_cast<type>(val); \
}
#  define SHMI_DEFINE_VARIANT_VALUE_STR_CAST_CHECKED() SHMI_DEFINE_VARIANT_VALUE_STR_CAST_UNCHECKED()

#  ifdef _DEBUG
#    define SHMI_DEFINE_VARIANT_VALUE_INT_CAST(type) SHMI_DEFINE_VARIANT_VALUE_INT_CAST_CHECKED(type)
#    define SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(type) SHMI_DEFINE_VARIANT_VALUE_UINT_CAST_CHECKED(type)
#    define SHMI_DEFINE_VARIANT_VALUE_FLT_CAST(type) SHMI_DEFINE_VARIANT_VALUE_FLT_CAST_CHECKED(type)
#    define SHMI_DEFINE_VARIANT_VALUE_STR_CAST() SHMI_DEFINE_VARIANT_VALUE_STR_CAST_CHECKED()
#  else
#    define SHMI_DEFINE_VARIANT_VALUE_INT_CAST(type) SHMI_DEFINE_VARIANT_VALUE_INT_CAST_UNCHECKED(type)
#    define SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(type) SHMI_DEFINE_VARIANT_VALUE_UINT_CAST_UNCHECKED(type)
#    define SHMI_DEFINE_VARIANT_VALUE_FLT_CAST(type) SHMI_DEFINE_VARIANT_VALUE_FLT_CAST_UNCHECKED(type)
#    define SHMI_DEFINE_VARIANT_VALUE_STR_CAST() SHMI_DEFINE_VARIANT_VALUE_STR_CAST_UNCHECKED()
#  endif

template<typename _Ty>
inline _Ty variant_value_cast(const variant_value & value);

template<>
inline bool variant_value_cast<bool>(const variant_value & value) {
	return value.toBool();
}

SHMI_DEFINE_VARIANT_VALUE_INT_CAST(char);
SHMI_DEFINE_VARIANT_VALUE_INT_CAST(short);
SHMI_DEFINE_VARIANT_VALUE_INT_CAST(int);
SHMI_DEFINE_VARIANT_VALUE_INT_CAST(long);
SHMI_DEFINE_VARIANT_VALUE_INT_CAST(long long);

SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(unsigned char);
SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(unsigned short);
SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(unsigned int);
SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(unsigned long);
SHMI_DEFINE_VARIANT_VALUE_UINT_CAST(unsigned long long);

SHMI_DEFINE_VARIANT_VALUE_FLT_CAST(float);
SHMI_DEFINE_VARIANT_VALUE_FLT_CAST(double);

SHMI_DEFINE_VARIANT_VALUE_STR_CAST();

// Cleanup PP
#  undef SHMI_MAP_TYPE_TO_VARIANT_VALUE_TYPE
#  undef SHMI_DEFINE_VARIANT_VALUE_INT_CAST_UNCHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_UINT_CAST_UNCHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_FLT_CAST_UNCHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_STR_CAST_UNCHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_INT_CAST_CHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_UINT_CAST_CHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_FLT_CAST_CHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_STR_CAST_CHECKED
#  undef SHMI_DEFINE_VARIANT_VALUE_INT_CAST
#  undef SHMI_DEFINE_VARIANT_VALUE_UINT_CAST
#  undef SHMI_DEFINE_VARIANT_VALUE_FLT_CAST
#  undef SHMI_DEFINE_VARIANT_VALUE_STR_CAST

} // namespace shmi

#endif // SHMI_VARIANT_VALUE_HELPERS_H
