//===-- variant_value.h - variant_value class definition --------*- C++ -*-===//
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
/// This file contains the declaration of the `variant_value` class.
///
/// \copyright
/// Smart HMI GmbH, 2013 - 2018
///
/// \date
/// 2016-11-17
///
/// \author
/// Tobias Kux <kux@smart-hmi.de>
///
//===----------------------------------------------------------------------===//
#ifndef SHMI_VARIANT_VALUE_H
#  define SHMI_VARIANT_VALUE_H
#  pragma once

#  include <memory>
#  include <cstdint>
#  include <string>
#  include <ostream>

#  include "shmi_cxx11_compat.h"

namespace shmi {

/*!
 * Variant type that holds item values.
 * This variant can be one of the following types:
 * * boolean
 * * signed integer
 * * unsigned integer
 * * double
 * * string
 */
class variant_value {
private:
	struct is_reference_t {};

public:
	enum value_type { kString = 0, kBool, kInt, kDouble, kUInt, kUndefined, kErr, kObject, kMethod, kAsyncMethod, kStringReference, kWideString };
#  ifdef SHMI_HAS_CXX11
	static constexpr is_reference_t is_reference{};
#  else
	static is_reference_t is_reference;
#  endif

public:
	//! Default constructor.
	/*!
	 * Creates a variant with an uninitialized type.
	 * All internal variables are initialized with 0 (or empty string) for safety.
	 */
	SHMI_CONSTEXPR variant_value() SHMI_NOEXCEPT : type_(kUndefined), iVal_(0) {}
	SHMI_CONSTEXPR variant_value(std::nullptr_t) SHMI_NOEXCEPT : type_(kUndefined), iVal_(0) {}
	//! Construct a variant containing the boolean value.
	SHMI_CONSTEXPR variant_value(bool val) SHMI_NOEXCEPT : type_(kBool), bVal_(val) {}
	//! Construct a variant containing the signed integer value.
	/*!
	 * \internal Even though we have a constructor for 64 bit integers,
	 * we're required to define one for 32bit as well because
	 * C++ type-promotion would consider promoting the 32bit integer
	 * to double AND long long and thus fail.
	 */
	SHMI_CONSTEXPR variant_value(int val) SHMI_NOEXCEPT : type_(kInt), iVal_(val) {}
	//! Construct a variant containing the unsigned integer value.
	/*!
	 * \internal Even though we have a constructor for 64 bit integers,
	 * we're required to define one for 32bit as well because
	 * C++ type-promotion would consider promoting the 32bit integer
	 * to double AND unsigned long long and thus fail.
	 */
	SHMI_CONSTEXPR variant_value(unsigned int val) SHMI_NOEXCEPT : type_(kUInt), uiVal_(val) {}
	//! Construct a variant containing the long value.
	SHMI_CONSTEXPR variant_value(long val) SHMI_NOEXCEPT : type_(kInt), iVal_(val) {}
	//! Construct a variant containing the unsigned long value.
	SHMI_CONSTEXPR variant_value(unsigned long val) SHMI_NOEXCEPT : type_(kUInt), uiVal_(val) {}
	//! Construct a variant containing the signed integer value.
	SHMI_CONSTEXPR variant_value(long long val) SHMI_NOEXCEPT : type_(kInt), iVal_(val) {}
	//! Construct a variant containing the unsigned integer value.
	SHMI_CONSTEXPR variant_value(unsigned long long val) SHMI_NOEXCEPT : type_(kUInt), uiVal_(val) {}
	//! Construct a variant containing the floating-point value.
	SHMI_CONSTEXPR variant_value(double val) SHMI_NOEXCEPT : type_(kDouble), fVal_(val) {}
	//! Construct a variant containing the string value.
	variant_value(const std::string & val) : type_(kString), sVal_(val) {}
	variant_value(const std::wstring & val) : type_(kWideString), wsVal_(val) {}
#  ifdef SHMI_HAS_CXX11
	//! Construct a variant containing the string value.
	/*!
	 * The `std::string` is moved into the internal storage (instead of copied)
	 * which is faster.
	 */
	variant_value(std::string && val) : type_(kString), sVal_(std::move(val)) {}
	variant_value(std::wstring && val) : type_(kWideString), wsVal_(std::move(val)) {}
#  endif
	//! Construct a variant containing the string value.
	variant_value(const char * const val) : type_(kString), sVal_(val) {}
	variant_value(const char * const val, std::size_t len) : type_(kString), sVal_(val, len) {}
	variant_value(const char * const val, is_reference_t) SHMI_NOEXCEPT : type_(kStringReference), srVal_(val) {}
	variant_value(const wchar_t * const val) : type_(kWideString), wsVal_(val) {}
	variant_value(const wchar_t * const val, std::size_t len) : type_(kWideString), wsVal_(val, len) {}
	variant_value(const variant_value & other);
#  ifdef SHMI_HAS_CXX11
	variant_value(variant_value && other) SHMI_NOEXCEPT;
#  endif
	~variant_value();

	void reset() noexcept;

	variant_value & operator=(const variant_value & other);
#  ifdef SHMI_HAS_CXX11
	variant_value & operator=(variant_value && other) SHMI_NOEXCEPT;
#  endif
	//! Compare two variants.
	/*!
	* \note The variants type has to match in order for the variants to be equal.
	*/
	bool operator==(const variant_value & other) const;
	//! Compare two variants.
	/*!
	* \see `operator==`
	*/
	bool operator!=(const variant_value & other) const;
	//!@{
	//! Convert the variant to a string representation.
	std::string str() const;
	std::wstring wstr() const;
#if defined(UNICODE) && defined(_WIN32)
	std::wstring tstr() const { return wstr(); }
#else
	std::string tstr() const { return str(); }
#endif
	//!@}

	SHMI_CONSTEXPR bool getBool() const SHMI_NOEXCEPT { return bVal_; }
	SHMI_CONSTEXPR std::int64_t getInt() const SHMI_NOEXCEPT { return iVal_; }
	SHMI_CONSTEXPR std::uint64_t getUInt() const SHMI_NOEXCEPT { return uiVal_; }
	SHMI_CONSTEXPR double getDouble() const SHMI_NOEXCEPT { return fVal_; }
	SHMI_CONSTEXPR const char * getStringRef() const SHMI_NOEXCEPT { return srVal_; }

	bool toBool() const;
	std::int64_t toInt() const;
	std::uint64_t toUInt() const;
	double toDouble() const;
	variant_value toType(value_type type) const;

	SHMI_CONSTEXPR bool & getRefBool() SHMI_NOEXCEPT { return bVal_; }
	SHMI_CONSTEXPR std::int64_t & getRefInt() SHMI_NOEXCEPT { return iVal_; }
	SHMI_CONSTEXPR std::uint64_t & getRefUInt() SHMI_NOEXCEPT { return uiVal_; }
	SHMI_CONSTEXPR double & getRefDouble() SHMI_NOEXCEPT { return fVal_; }
	SHMI_CONSTEXPR std::string & getRefString() SHMI_NOEXCEPT { return sVal_; }
	SHMI_CONSTEXPR std::wstring & getRefWideString() SHMI_NOEXCEPT { return wsVal_; }

	SHMI_CONSTEXPR const bool & getRefBool() const SHMI_NOEXCEPT { return bVal_; }
	SHMI_CONSTEXPR const std::int64_t & getRefInt() const SHMI_NOEXCEPT { return iVal_; }
	SHMI_CONSTEXPR const std::uint64_t & getRefUInt() const SHMI_NOEXCEPT { return uiVal_; }
	SHMI_CONSTEXPR const double & getRefDouble() const SHMI_NOEXCEPT { return fVal_; }
	SHMI_CONSTEXPR const std::string & getRefString() const SHMI_NOEXCEPT { return sVal_; }
	SHMI_CONSTEXPR const std::wstring & getRefWideString() const SHMI_NOEXCEPT { return wsVal_; }

	SHMI_EXPLICIT_OPERATOR SHMI_CONSTEXPR operator bool() const SHMI_NOEXCEPT { return bVal_; }
	SHMI_EXPLICIT_OPERATOR SHMI_CONSTEXPR operator std::int64_t() const SHMI_NOEXCEPT { return iVal_; }
	SHMI_EXPLICIT_OPERATOR SHMI_CONSTEXPR operator std::uint64_t() const SHMI_NOEXCEPT { return uiVal_; }
	SHMI_EXPLICIT_OPERATOR SHMI_CONSTEXPR operator double() const SHMI_NOEXCEPT { return fVal_; }

	SHMI_CONSTEXPR value_type type() const SHMI_NOEXCEPT { return type_; }

	static variant_value parse(const std::string & str, value_type type);

	static value_type type_from_string(const std::string & str);

private:
	void p_destroy() noexcept;

private:
	//! Type of the variant
	value_type type_;
	// elemental values are stored in a union to save memory
	union {
		//! Boolean value of the variant.
		bool bVal_;
		//! Signed integer value of the variant.
		std::int64_t iVal_;
		//! Unsigned integer value of the variant.
		std::uint64_t uiVal_;
		//! Floating-point value of the variant.
		double fVal_;
		//! String value of the variant.
		std::string sVal_;
		std::wstring wsVal_;
		const char * srVal_;
	};
};

std::ostream & operator<<(std::ostream & strm, const variant_value & value);

} // namespace shmi

namespace std {
string to_string(shmi::variant_value::value_type val);
}

#endif // SHMI_VARIANT_VALUE_H
