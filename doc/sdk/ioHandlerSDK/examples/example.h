//===-- ioHandler.cpp - ioHandlerExample class declaration ------*- C++ -*-===//
//
//                         WebIQ Connect ioHandler SDK
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
/// This file contains the definition of the ioHandlerExample class.
///
/// \copyright
/// Smart HMI GmbH, 2016 - 2018
///
/// \date
/// 2016-11-17
///
/// \author
/// Tobias Kux <kux@smart-hmi.de>
///
//===----------------------------------------------------------------------===//
#ifndef SHMI_IOHANDLER_EXAMPLE_EXAMPLE_H
#  define SHMI_IOHANDLER_EXAMPLE_EXAMPLE_H
#  pragma once

#  include <atomic>
#  include <chrono>
#  include <condition_variable>
#  include <cstdint>
#  include <ctime>
#  include <map>
#  include <mutex>
#  include <thread>
#  include <shmi/sdk/ioHandler.h>
#  include <shmi/variant_value.h>
#  include <string>
#  include <unordered_map>
#  include <vector>

#  include "ioHandlerBase.h"
#  include "method.h"
#  include "SubscriptionGroup.h"

#  define myHandler "ioHandlerExample v1.0"

/// This class implements the example ioHandler.
/// This ioHandler provides 10 variables, called "var0" to "var9".
/// * "var0" and "var1" are of type int, have a starting value of 0 and are
///		incremented by one every 100 milliseconds.
/// * "var2" and "var3" are of type double, have a starting value of 0 and are
///		incremented by 0.3 every 100 milliseconds.
/// * "var4" to "var9" do not have a simulation function attached to them, but
///		they can be read and written to by WebIQ Connect.
///
/// When reaching a value greater than 100, "var0" to "var3" are reset to their
///	starting values (0) respectively.
///
/// Upon instantiation, two threads are spawned:
/// * The simulation thread `ioHandlerExample::thread_proc_simulation`.
/// * The monitoring thread `ioHandlerExample::thread_proc_monitor`.
///
/// The simulation thread wakes up every 100ms to run one simulation cycle.
/// The monitoring thread is used to check the variables for updates and send
/// them to WebIQ connect. This thread sleeps for as long as needed, until new
/// data is expected. This means that the monitoring thread sleeps
/// indefinitely, if there are no subscriptions, thus minimizing system load.
class ioHandlerExample : public shmi::ioHandlerBase
{
	friend class SubscriptionGroup;
public:
	using duration_type = std::chrono::milliseconds;
	using clock_type = std::chrono::system_clock;
	using timestamp_type = clock_type::time_point;

private:
	/// The simulation thread procedure.
	///
	/// This simulation function runs every 100ms and updates variable 0 to 4.
	/// * Integral variables are incremented by one and set to 0 once their
	///		value exceeds 100.
	/// * Floating-point variables are incremented by 0.3 and set to 0 once
	///		their value exceeds 100.
	/// * Boolean variables are flipped.
	/// * String variables have an internal integer counter that is
	///		incremented by once and set to 0 once their value exceeds 100. The
	///		string value is "Value " + counter.
	void thread_proc_simulation();

	/// The monitor thread procedure.
	///
	/// This monitoring function sleeps for as long as no updates are required.
	/// If a `SubscriptionGroup` requires an update this thread wakes up,
	///	updates the neccessary items (if required) and then goes to sleep again
	/// until any `SubscriptionGroup` needs to be polled for updates again.
	void thread_proc_monitor();

	/// Gets the `SubscriptionGroup` for the desired refresh interval.
	/// If the `SubscriptionGroup` does not exist yet, it is created.
	///
	/// \return Reference to the `SubscriptionGroup` with the desired refresh interval.
	SubscriptionGroup & get_subscription_group(std::chrono::milliseconds interval);

	/// Gets a reference to an existing items value.
	///
	/// \param[in] item_name The name of the existing item.
	///
	/// \return reference to the existing items value.
	///
	/// \throw `std::invalid_argument` if the item does not exist.
	shmi::variant_value & get_item_value(const std::string & item_name);

public:
	/// Constructs an instance of the ioHandler.
	///
	/// \param[in] param `ioHandlerParam` struct that is passed by
	/// WebIQ Connect during `CreateIoInstance()`.
	ioHandlerExample(const ioHandlerParam * const param);

	/// Destroys the ioHandler.
	/// `ioHandlerStub2::terminate()` is called before the ioHandler is
	/// destroyed.
	~ioHandlerExample();

	/// Notifies all threads to shut down and joins with them.
	void terminate();

	/// Subscribe number of items.
	///
	/// \param[in] interval The refresh interval of the items in milliseconds.
	/// \param[in] symbols The names of the items to subscribe.
	/// \param[out] types A vector holding pointers to the types of the subscribed
	///		items. If an item could not be subscribed, its type is VAR_ERR. If
	///		an items type can not be determined immediately, its type is VAR_UNDEFINED.
	///
	/// \return 0 on success, non-zero on error.
	int subscribe(std::chrono::milliseconds interval, const std::vector<std::string> & symbols, const std::vector<int*> & types);

	/// Subscribe number of items.
	///
	/// \param[in] interval The refresh interval of the items in milliseconds.
	/// \param[in] symbols Pointer to an array of item names to subscribe.
	/// \param[out] types An array of ints, holding the types of the subscribed
	///		items. If an item could not be subscribed, its type is VAR_ERR. If
	///		an items type can not be determined immediately, its type is
	///		VAR_UNDEFINED.
	/// \param[in] numSymbols Size of the array of item names.
	///
	/// \return 0 on success, non-zero on error.
	int subscribe(std::chrono::milliseconds interval, const char *const * symbols, int * types, std::size_t numSymbols);

	/// Unsubscribe a number of items
	///
	/// \param[in] symbols The names of the items to ubsubscribe.
	///
	/// \return 0 on success, non-zero on error.
	///
	/// \note
	///		This removes the items from all `SubscriptionGroups`, regardless of
	///		how often they've been subscribed.
	int unsubscribe(const std::vector<std::string> & symbols);

	/// Unsubscribe number of items.
	///
	/// \param[in] symbols Pointer to an array of item names to unsubscribe.
	/// \param[in] numSymbols Size of the array of item names.
	///
	/// \return 0 on success, non-zero on error.
	///
	/// \note
	///		This removes the items from all `SubscriptionGroups`, regardless of
	///		how often they've been subscribed.
	int unsubscribe(const char *const * symbols, std::size_t numSymbols);

	/// Write a value into an item. The new value has to be given in string
	/// representation and is then converted into the items type. If the item
	/// does not exist or type conversion fails, this function fails.
	///
	/// \param[in] symbol Name of an item.
	/// \param[in] value The value to be assigned to the item.
	///
	/// \return 0 on success, non-zero on error.
	int write(const std::string & symbol, const ioDataValue & value);

	/// Synchronously read an item value.
	/// The item value is directly sent to WebIQ Connect and not returned by
	/// this function.
	///
	/// \param[in] symbol Name of an item.
	///
	/// \return 0 on success, non-zero on error.
	int read(const std::string & symbol);

	/// Call a method by name.
	///
	/// \param[in] context The context pointer passed by WebIQ Connect.
	/// \param[in] methodName The name of the method(s) to call.
	/// \param[in] methodArguments The arguments that are passed to the
	///		called method(s). Must be a valid json string.
	///
	/// \return 0 on success, non-zero on error.
	///
	/// \see methodExample::call
	int method(const void * context, const std::string & methodName, const std::string & methodArguments);

private:
	/// The method handler stub.
	methodExample methodHandler_;

	/// Indicates whether the threads should terminate or not.
	/// `true` = terminate, `false` = continue.
	/// Is initialized with `false`
	std::atomic_bool terminate_{ false };

	/// Thread handle to the simulation thread.
	std::thread thrSimulation_;
	/// Thread handle to the monitoring/update thread.
	std::thread thrMonitor_;
	/// Mutex that is used to guard threaded access of internal objects.
	std::mutex mutex_;
	/// Condition variable to wake up the simulation thread in case it has to be terminated.
	std::condition_variable condSimWakeup_;
	/// Condition variable to wake up the monitoring/update thread in case it has to be terminated
	/// or a new `SubscriptionGroup` has been registered.
	std::condition_variable condMonitorWakeup_;

	/// Map of `SubscriptionGroup`s.
	std::map<std::chrono::milliseconds, SubscriptionGroup> subscriptionGroups_;
	/// Stores all items of this ioHandler
	std::unordered_map<std::string, shmi::variant_value> items_;
};

#endif // SHMI_IOHANDLER_EXAMPLE_EXAMPLE_H
