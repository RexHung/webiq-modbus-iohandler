//===-- ioHandler.h - ioHandler C API definition ----------------*- C++ -*-===//
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
/// Contains the definition of the ioHandler interface to connect process
/// variables and methods from arbitrary sources to the WebIQ Connect Server.
/// The ioHandler is compiled and linked as shared library and exposes a
/// C-language compatible interface. The shared library will be loaded by the
/// WebIQ Connect Server during runtime if configured/enabled. An ioHandler can
/// be configured by creating an appropriate table row in the [_Config_IOHandler]
/// table found in the project SQLite database (project.sqlite by default).
///
/// See `::pIoCreate` for more information on the available configuration options.
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
#ifndef SHMI_IOHANDLER_H
#  define SHMI_IOHANDLER_H
#  pragma once

#  define RESULT_OK 0
#  define RESULT_FAIL 1
#  define RESULT_NOT_IMPLEMENTED

#  ifdef __cplusplus
#    define SHMI_EXTERN_C extern "C"
#  else
#    define SHMI_EXTERN_C
#  endif /* __cplusplus */

#  ifdef _MSC_VER
#    define DLL_EXPORT SHMI_EXTERN_C __declspec(dllexport)
#  elif defined(__GNUC__)
#    if defined(_WIN32) || defined(__CYGWIN__)
#      define DLL_EXPORT SHMI_EXTERN_C __attribute__ ((dllexport))
#    else
#      define DLL_EXPORT SHMI_EXTERN_C __attribute__ ((visibility ("default")))
#    endif
#  else
#    define DLL_EXPORT SHMI_EXTERN_C
#  endif

#  if defined(_MSC_VER)
#    define SHMI_VALIDATE_FORMAT(archetype, idx, first)
#    define SHMI_DEPRECATED __declspec(deprecated)
#  elif defined(__GNUC__)
#    define SHMI_VALIDATE_FORMAT(archetype, idx, first) __attribute__ ((format (archetype, idx, first)))
#    define SHMI_DEPRECATED __attribute__ ((deprecated))
#  else
#    define SHMI_VALIDATE_FORMAT(archetype, idx, first)
#    define SHMI_DEPRECATED
#  endif

#  if _MSC_VER >= 1400
#    include <sal.h>
#    if _MSC_VER <= 1400
#      define _Printf_format_string_ __format_string
#    endif
#    ifndef _Outptr_
#      define _Outptr_ _Deref_out_
#    endif
#  else
#    define __checkReturn
#    define _Printf_format_string_
#    define _In_
#    define _In_z_
#    define _In_opt_
#    define _Inout_
#    define _Out_
#    define _Outptr_
#  endif /* _MSC_VER */

/**
 * Helper macro for creating `ioVersion` values.
 */
#  define SHMI_MAKE_IOVERSION(major, minor, patch, build) \
	((ioVersion)( \
		(unsigned long long)((major) & 0xFFFF) << 48 | \
		(unsigned long long)((minor) & 0xFFFF) << 32 | \
		(unsigned long long)((patch) & 0xFFFF) << 16 | \
		(unsigned long long)((build) & 0xFFFF) \
	))

/**
 * Oldest API version number that is compatible with the API defined in this
 * header.
 */
#  define SHMI_IOHANDLER_API_COMPATIBLE_VERSION SHMI_MAKE_IOVERSION(1, 4, 0, 0)

#  include <stddef.h>

/**
 * Supported intrinsic datatypes.
 */
enum enumtype {
	/**
	 * The value is a string.
	 */
	VARSTRING = 0,
	/**
	 * The value is a boolean.
	 *
	 * \internal
	 *	Booleans are represented as (signed) integers when being passed between
	 *	WebIQ Connect and ioHandlers.
	 */
	VARBOOL,
	/**
	 * The value is a signed integer. Up to 64bit integers are supported.
	 */
	VARINT,
	/**
	 * The value is a floating-point value. Double-precision is always used.
	 */
	VARFLOAT,
	/**
	 * The value is an unsigned integer. Up to 64bit integers are supported.
	 */
	VARUINT,
	/**
	 * The type of the value is undefined. A value is undefined if no value has
	 * been received yet or if an ioHandler wants to invalidate any values.
	 */
	VAR_UNDEFINED,
	/**
	 * The value type is used to indicate an error during item subscription.
	 */
	VAR_ERR,
	/**
	 * Reserved.
	 */
	VAR_OBJECT,
	/**
	 * The value represents a method callback value. This value should not
	 * be used with actual `ioDataValue`s and is only used internally.
	 */
	VAR_METHOD
};

/**
 * Signals to signal intents or states to WebIQ Connect.
 */
enum ioHandlerSignal {
	/**
	 * Causes WebIQ Connect to shutdown. Does not take any arguments.
	 */
	IOSIG_SHUTDOWN = 1,
	/**
	 * Causes WebIQ Connect to restart. Does not take any arguments.
	 */
	IOSIG_RESTART,
	/**
	 * Notifies WebIQ Connect about the ioHandlers connection status. Takes an
	 * integer as argument.
	 * <table>
	 *	<tr><td>&lt;0</td><td>Unknown connection state.</td></tr>
	 *	<tr><td>0</td><td>ioHandler is disconnected.</td></tr>
	 *	<tr><td>&gt;0</td><td>ioHandler is connected.</td></tr>
	 * </table>
	 */
	IOSIG_CONNECTIVITY,
};

/**
 * WebIQ Connect logging severities.
 */
enum ioLogSeverity {
	/**
	 * Very verbose debugging output, which may be too much to print to console
	 * but might be useful to have in a logfile.
	 */
	IOLOG_TRACE,
	/**
	 * Basic debugging output. This output is not usually meant to be seen by
	 * end-users.
	 */
	IOLOG_DEBUG,
	/**
	 * Informational output. Output with this and higher severity levels are
	 * intended to be seen by the end-user.
	 */
	IOLOG_INFO,
	/**
	 * Severity for important information.
	 */
	IOLOG_NOTICE,
	/**
	 * Output for warnings. Warnings occur when an irregularity/error occurs,
	 * which can be handled with little to none impact to the expected
	 * behaviour.
	 */
	IOLOG_WARN,
	/**
	 * Output for errors which cause the related operation to fail.
	 */
	IOLOG_ERR,
	/**
	 * Output for errors which causes the related operation to fail and may
	 * impact further operations as well. After a critical error, the program
	 * may be unable to guarantee to behave according to its specification.
	 * Fatal errors may follow.
	 */
	IOLOG_CRIT,
	/**
	 * Output for fatal errors. Fatal errors cause the program to fail and
	 * terminate. During initialization, ioHandlers may also use this when
	 * `CreateIoInstance` is going fail.
	 *
	 * \note
	 *	Writing a log message with `IOLOG_FATAL` does not cause WebIQ Connect
	 *	to terminate by itself. The ioHandler is reponsible for either sending
	 *	`IOSIG_SHUTDOWN` or terminating the process on its own afterwards.
	 */
	IOLOG_FATAL,
	/**
	 * Output for audit log messages. Messages for this severity will not be
	 * output to the log file or the console but instead will be inserted as
	 * records in the audit log. The log record will have its event name set to
	 * the name of the ioHandler, its user set to `system` and its `custom`
	 * flag will be cleared. If the name of the ioHandler could not be deduced
	 * (because no handle was provided), "ioHandler" will be used as event name
	 * instead.
	 *
	 * \note
	 *	This severity level might not be available during calls to
	 *	`CreateIoInstance` and `DestroyIoInstance`.
	 */
	IOLOG_AUDIT,
};

/**
 * Type used for storing the ioHandlers version. Use the
 * `SHMI_MAKE_IOVERSION` macro to generate a valid version.
 */
typedef unsigned long long ioVersion;

/**
 * Variant type for value exchange between WebIQ Connect and an ioHandler.
 */
struct ioDataValue {
	//! Type of the value held by the variant.
	enumtype varType;

	union {
		//! Signed integer value of the variant type.
		long long iVal;
		//! Unsigned integer value of the variant type.
		unsigned long long uiVal;
		//! Floating-point value of the variant type.
		double dVal;
		//! Pointer to the string held by the variant type.
		const char* sVal;
	};

	//! Index of an array (1....) or 0 for a single item
	int index;
	//! Reserved
	int status;
};

/**
 * Callback type used to pass item values to WebIQ Connect.
 *
 * \param[in] item Name of the item the updated value is for.
 * \param[in] value Pointer to an `ioDataValue` holding the new value for the
 *	item.
 * \param[in] handle Handle provided to the ioHandler via the `ioHandlerParam`
 *	struct during initialization.
 */
typedef void tReadCallback(
	_In_z_ const char* item,
	_In_ const ioDataValue * value,
	_In_ void * handle
);

/**
 * Callback type used to pass method results to WebIQ Connect.
 *
 * \param[in] context Context pointer that has been provided by WebIQ Connect
 *	when the method has been called initially.
 * \param[in] value Method result.
 * \param[in] handle Handle provided to the ioHandler via the `ioHandlerParam`
 *	struct during initialization.
 */
typedef void tFunctionCallback(
	_In_ const void * context,
	_In_z_ const char * value,
	_In_ void * handle
);

/**
 * Callback type used to signal intents or states to WebIQ Connect.
 * The number of parameters is dependent on the type of signal sent.
 *
 * \param[in] handle Handle provided to the ioHandler via the `ioHandlerParam`
 *	struct during initialization.
 * \param[in] signal Signal index.
 */
typedef void tSignalCallback(
	_In_ void * handle,
	_In_ ioHandlerSignal signal,
	...
);

/**
 * Print a log message with the given severity to the WebIQ Connect Log-Console.
 * The log message will be assigned to a channel with the same name as the
 * ioHandler.
 *
 * \param[in] handle Handle provided to the ioHandler via the `ioHandlerParam`
 *	struct during initialization. If for whatever reason no valid handle can be
 *	provided, `NULL` may be used instead. In this case WebIQ Connect will not be
 *	able assign a proper channel to the log message and "ioHandler" will be
 *	used instead.
 * \param[in] severity Severity of the log message. Invalid severities are
 *	treated as `IOLOG_INFO`.
 * \param[in] str String to be written to the WebIQ Connect log.
 */
typedef void tIoLog(
	_In_opt_ void * handle,
	_In_ ioLogSeverity severity,
	_In_z_ const char * str
);

/**
 * Format and print a log message with the given severity to the WebIQ Connect
 * Log-Console. The log message will be assigned to a channel with the same name
 * as the ioHandler.
 *
 * \param[in] handle Handle provided to the ioHandler via the `ioHandlerParam`
 *	struct during initialization. If for whatever reason no valid handle can be
 *	provided, `NULL` may be used instead. In this case WebIQ Connect will not be
 *	able assign a proper channel to the log message and "ioHandler" will be
 *	used instead.
 * \param[in] severity Severity of the log message. Invalid severities are
 *	treated as `IOLOG_INFO`.
 * \param[in] format Pointer to a null-terminated multibyte string specifying
 *	how to interpret the data. The format string consists of ordinary multibyte
 *	characters (except %), which are copied unchanged into the output stream,
 *	and conversion specifications.
 *
 * \see
 *	For a full documentation on the format string please refer to
 *	http://en.cppreference.com/w/c/io/fprintf
 */
typedef void tIoLogf(
	_In_opt_ void * handle,
	_In_ ioLogSeverity severity,
	_In_z_ _Printf_format_string_ const char * format,
	...
) SHMI_VALIDATE_FORMAT(__printf__, 3, 4);

/**
 * Print Debug messagges to the WebIQ Connect Log-Console. Other than using
 * `tIoLog` or `tIoLogf`, WebIQ Connect will not be able to identify the
 * ioHandler printing the log message. Instead of the ioHandlers name, the
 * log channel "ioHandler" will be used instead. All messages will be assigned
 * the `IOLOG_INFO` severity.
 *
 * \param[in] format Pointer to a null-terminated multibyte string specifying
 *	how to interpret the data. The format string consists of ordinary multibyte
 *	characters (except %), which are copied unchanged into the output stream,
 *	and conversion specifications.
 *
 * \see
 *	For a full documentation on the format string please refer to
 *	http://en.cppreference.com/w/c/io/fprintf
 *
 * \deprecated
 *	Usage of this message is deprecated due to its interface limitations. Use
 *	`tIoLog` or `tIoLogf` instead.
 */
typedef void DebugLog(
	_In_z_ _Printf_format_string_ const char * format,
	...
) SHMI_VALIDATE_FORMAT(__printf__, 1, 2);

/**
 * Data structure holding initialization values passed from WebIQ Connect
 * to ioHandlers during instantiation.
 */
struct ioHandlerParam {
	/**
	 * Function pointer to WebIQ Connects printf-like debug message interace.
	 */
	SHMI_DEPRECATED DebugLog* const logprintf;
	/**
	 * Handle that has to be passed to any call to `readCallback` or
	 * `functionCallback`.
	 */
	void * const handle;
	/**
	 * Function pointer to WebIQ Connects item value callback.
	 * \see `tReadCallback`.
	 */
	tReadCallback* const readCallback;
	/**
	 * Function pointer to WebIQ Connects method result callback.
	 * \see `tFunctionCallback`.
	 */
	tFunctionCallback* const functionCallback;
	/**
	 * Stringyfied configuration parameters read from the ioHandler
	 * configuration.
	 *
	 * \note
	 *	A `NULL` value in the configuration database will cause the
	 *	corresponding entry in this array to also be `NULL`.
	 */
	const char* const param[10];
	/**
	 * Function pointer to WebIQ Connects signal callback.
	 * \see `tSignalCallback`.
	 */
	tSignalCallback* const signalCallback;
	/**
	 * Function pointer to WebIQ Connects string logging function.
	 */
	tIoLog* const ioLog;
	/**
	 * Function pointer to WebIQ Connects formatting string logging function.
	 */
	tIoLogf* const ioLogf;
};

/**
 * Data structure holding information about a single parameter from the
 * `ioHandlerParam::param` array.
 */
struct ioHandlerParameterDescription {
	/**
	 * Name of the parameter. May be `NULL`.
	 */
	const char * paramName;
	/**
	 * Description for the parameters functionality. May be `NULL`.
	 */
	const char * paramDescription;
	/**
	 * Indicates whether the parameter is optional (can be `NULL`) or not.
	 * If this value is non-zero, WebIQ Connect v1.5 and higher will refuse to
	 * load the ioHandler if the value of this parameter is `NULL`. This
	 * is only intended to be used for giving early user feedback. Checking the
	 * parameter values for `NULL` is still highly recommended.
	 */
	unsigned char optional;
};

/**
 * Data structure holding information about the ioHandler.
 */
struct ioHandlerV2Info {
	/**
	 * Version of the struct. Must be `0x1000500000000ULL`.
	 */
	ioVersion structVersion;
	/**
	 * Version of the ioHandler API used. Must be set to
	 * `SHMI_IOHANDLER_API_COMPATIBLE_VERSION`.
	 */
	ioVersion compatibleApiVersion;
	/**
	 * Name of the ioHandler. Must not be `NULL`.
	 */
	const char * ioHandlerName;
	/**
	 * Description of the ioHandler. May be `NULL`.
	 */
	const char * ioHandlerDescription;
	/**
	 * Version of the ioHandler. May be created with the `SHMI_MAKE_IOVERSION`
	 * helper macro.
	 */
	ioVersion ioHandlerVersion;
	/**
	 * Pointer to an array containing `ioHanderParameterDescription` for each
	 * parameter used by the ioHandler. The array has to have at least
	 * `paramCount` elements. May be `NULL` to disable any kind of parameter
	 * validation or information passing for WebIQ Designer.
	 */
	const ioHandlerParameterDescription * parameterInfo;
	/**
	 * Maximal number of parameters accepted by the ioHandler. Must not exceed
	 * `10`. The number of elements in the array pointed to by `parameterInfo`
	 * must be greater or equal to this value.
	 */
	size_t parameterCount;
};

/**
 * Version to be used as `structVersion` value for the ioHandlerInfo` struct.
 */
#  define SHMI_IOHANDLERINFO_CURRENT_VERSION SHMI_MAKE_IOVERSION(1, 5, 0, 0)

/**
 * Type alias for the most recent ioHandler info struct. Using this type
 * instead of a specific info struct version is recommended. When using this
 * type, `structVersion` should always be initialized with
 * `SHMI_IOHANDLERINFO_CURRENT_VERSION`.
 */
typedef ioHandlerV2Info ioHandlerInfo;

/**
 * Function type for the `CreateIoInstance()` export function.
 *
 * \param[in] pszIPAddress IP-Address from [Config_IOHandler] table. May be
 *	used to pass an IP-address to connect to.
 * \param[in] sPort Port from [Config_IOHandler] table. May be used to pass a
 *	port to connect to.
 * \param[in] param Pointer to the `ioHandlerParam` structure.
 * \param[out] handle Handle (or pointer) to the instance of the new ioHandler
 *	object. This value will not be modified by WebIQ Connect and may be passed
 *	to the ioHandlers other exported functions.
 * \return 0 on success, non-zero on error.
 */
typedef int pIoCreate(
	_In_z_ const char* pszIPAddress,
	_In_ unsigned short iPort,
	_In_ const ioHandlerParam* params,
	_Outptr_ void ** handle
);

/**
 * Function type for the `DestroyIoInstance()` export function.
 *
 * \param[in] handle Handle (or pointer) to the instance of the ioHandler that
 *	is to be destroyed.
 * \return 0 on success, non-zero on error.
 */
typedef	int pIoDestroy(
	_Inout_ void * handle
);

/**
 * Function type for the `SubscribeItems()` export function.
 *
 * \param[in] llInterval Time in millisecond that the ioHandler should at least
 *	wait between passing values for the same item to WebIQ Connect.
 * \param[in] iNumOfSymbols Number of items in `ppszSymbolList` and `piItemTypes`.
 * \param[in] ppszSymbolList List of items to be subscribed
 * \param[out] piItemTypes List of each items type. If an item could not be
 *	subscribed, its type must be set to `VAR_ERR`.
 * \param[in] handle Handle (or pointer) to the instance of the ioHandler that
 *	is to subscribe the items.
 * \return 0 on success, non-zero on error.
 */
typedef	int pIoSubscribe(
	_In_ long long iInterval,
	_In_ size_t iNumOfSymbols,
	_In_ const char* const* ppszSymbolList,
	_Inout_ int* const type,
	_Inout_ void * handle
);


/**
 * Function type for the `UnsubscribeItems()` export function.
 *
 * \param[in] iNumOfSymbols Number of items in `ppszSymbolList`.
 * \param[in] ppszSymbolList List of items to be unsubscribed,
 * \param[in] handle Handle (or pointer) to the instance of the ioHandler that
 *	is to unsubscribe the items.
 * \return 0 on success, non-zero on error.
 */
typedef	int pIoUnsubscribe(
	_In_ size_t iNumOfSymbols,
	_In_ const char* const* ppszSymbolList,
	_Inout_ void * handle
);

/**
 * Function type for the `WriteItem()` export function.
 *
 * \param[in] pszWriteSymbol item (symbol) to write
 * \param[in] pszWriteValue Value
 * \param[in] index Index (array) or 0 (else)
 * \param[in] handle Handle (or pointer) to the instance of the ioHandler that
 *	is requested to do the write operation.
 * \return RESULT (zero on success, non-zero else)
 */
typedef	int pIoWrite(
	_In_ const char* pszWriteSymbol,
	_In_ const ioDataValue * pszWriteValue,
	_In_ int index,
	_Inout_ void * handle
);

/**
 * Function type for the `CallMethod()` export function.
 *
 * \param[in] pMethodContext Void-Pointer that may point to one of
 *	WebIQ Connects internal data structures. May not be altered and must be
 *	used as context parameter when calling `functionCallback::functionCallback`.
 * \param[in] pszMethodObject Optional method argument that can be configured
 *	in WebIQ Connect.
 * \param[in] pszMethod Name of the method to call
 * \param[in] pszInputArgs Input arguments as JSON-string
 * \param[in] handle Handle (or pointer) to the instance of the ioHandler that
 *	is requested to call the method.
 * \return 0 on success, non-zero on error.
 */
typedef	int pIoMethod(
	_In_ const void* pMethodContext,
	_In_ const char* pszMethodObject,
	_In_ const char* pszMethod,
	_In_ const char* pszWriteValue,
	_Inout_ void * handle
);

/**
 * Function type for the `ReadItem()` export function.
 *
 * \param[in] pszReadSymbol Name of the item to read.
 * \param[in] handle Handle (or pointer) to the instance of the ioHandler that
 *	is requested to do the read operation.
 * \return 0 on success, non-zero on error.
 */
typedef	int pIoRead(
	_In_ const char* pszReadSymbol,
	_Inout_ void * handle
);

/**
 * Function type for the `GetIoInfo()` export function.
 *
 * \return Pointer to the ioHandlers `ioHandlerInfo` struct. May return `NULL`.
 */
typedef __checkReturn const ioHandlerInfo * pIoGetInfo(
);

#endif // SHMI_IOHANDLER_H
