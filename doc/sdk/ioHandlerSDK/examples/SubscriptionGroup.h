//===-- SubscriptionGroup.h - SubscriptionGroup class def. ------*- C++ -*-===//
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
/// This file contains the definition of the `SubscriptionGroup` class.`
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
#ifndef SHMI_IOHANDLER_EXAMPLE_SUBSCRIPTIONGROUP_H
#  define SHMI_IOHANDLER_EXAMPLE_SUBSCRIPTIONGROUP_H
#  pragma once

#  include <chrono>
#  include <shmi/variant_value.h>
#  include <string>
#  include <unordered_map>

class ioHandlerExample;

/// A `SubscriptionGroup` manages the update of items with the same refresh
/// interval.
class SubscriptionGroup
{
public:
	using duration_type = std::chrono::milliseconds;
	using clock_type = std::chrono::system_clock;
	using timestamp_type = clock_type::time_point;

	/// Helper struct to store the last item value of a subscribed item.
	/// This struct is used to decide whether or not an item value has changed
	/// and if it's required to be resent.
	struct subscribed_item_value
	{
		/// Last item value
		shmi::variant_value last_value;
		/// If `true`, the item will be updated, regardless of its last value.
		bool force_refresh;
	};

public:
	/// Constructor.
	///
	/// \param[in] ioHandler Reference to the parent object (the ioHandler
	///		itself)
	/// \param[in] interval The refresh interval of the `SubscriptionGroup`
	SubscriptionGroup(ioHandlerExample & ioHandler, const duration_type & interval);

	/// Get the timestamp of the next tick.
	///
	/// \return timestamp of the next tick.
	const timestamp_type & get_next_tick() const;

	/// Do a tick.
	/// This will poll all member items for updates and increase the next tick
	/// timestamp by the `SubscriptionGroups` update interval regardless of the
	/// current time.
	void do_tick();

	/// Update a single item.
	/// This will cause an item to be updated with the next tick by setting
	/// its `subscribed_item_value::force_refresh` to true. If the item does
	/// not exist, this does nothing.
	///
	/// \param[in] itemName Name of the item to be updated.
	void refresh_single_item(const std::string & itemName);

	/// Adds an item to the `SubscriptionGroup`.
	///
	/// \param[in] itemName Name of the item to be added.
	void add_item(const std::string & itemName);
	
	/// Removes an item from the `SubscriptionGroup`.
	///
	/// \param[in] itemName Name of the item to be removed.
	void remove_item(const std::string & itemName);
	
	/// Checks if the `SubscriptionGroup` is empty or not.
	///
	/// \return `true` if the `SubscriptionGroup` is empty, `false` else.
	bool empty() const;

private:
	/// Reference to the parent object.
	ioHandlerExample & ioHandler_;
	/// Refresh interval of the underlying items.
	duration_type interval_;
	/// Timestamp of the next timepoint the `SubscriptionGroup`s items have to
	/// be polled for updates.
	timestamp_type nextTick_;
	/// Map of all member items with their last seen values.
	std::unordered_map<std::string, subscribed_item_value> items_;
};

#endif // SHMI_IOHANDLER_EXAMPLE_SUBSCRIPTIONGROUP_H
