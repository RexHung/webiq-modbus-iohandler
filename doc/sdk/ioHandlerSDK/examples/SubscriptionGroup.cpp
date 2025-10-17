//===-- SubscriptionGroup.cpp - SubscriptionGroup class impl. ---*- C++ -*-===//
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
/// This file contains the implementation of the `SubscriptionGroup` class.`
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
#include "SubscriptionGroup.h"
#include "example.h"

SubscriptionGroup::SubscriptionGroup(ioHandlerExample & ioHandler, const duration_type & interval)
	: ioHandler_(ioHandler)
	, interval_(interval)
	, nextTick_(clock_type::now())
{
}

const SubscriptionGroup::timestamp_type & SubscriptionGroup::get_next_tick() const {
	return nextTick_;
}

void SubscriptionGroup::do_tick() {
	for ( auto & item_pair : items_ ) {
		try {
			auto current_item_value = ioHandler_.get_item_value(item_pair.first);
			// Force refresh or value change?
			if ( item_pair.second.force_refresh || current_item_value != item_pair.second.last_value ) {
				// Send updated value to WebIQ Connect.
				ioHandler_.send_item_value(item_pair.first,	current_item_value);
				item_pair.second.force_refresh = false;
				item_pair.second.last_value = std::move(current_item_value);
			}
		} catch ( const std::invalid_argument & ) {
		}
	} // for ( auto & item_pair : items_ )
	nextTick_ += interval_;
}

void SubscriptionGroup::refresh_single_item(const std::string & itemName) {
	auto iter = items_.find(itemName);
	if ( iter != items_.end() )
		iter->second.force_refresh = true;
}

void SubscriptionGroup::add_item(const std::string & itemName) {
	auto iter = items_.find(itemName);
	if ( iter == items_.end() ) {
		// Create a new item with force_refresh set to true,
		// so the first value is directly sent to WebIQ Connect.
		items_[itemName] = { { }, true };
	}
}

void SubscriptionGroup::remove_item(const std::string & itemName) {
	items_.erase(itemName);
}

bool SubscriptionGroup::empty() const {
	return items_.empty();
}
