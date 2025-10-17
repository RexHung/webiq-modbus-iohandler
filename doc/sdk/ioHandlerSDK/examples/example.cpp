//===-- ioHandler.cpp - ioHandlerExample class implementation ---*- C++ -*-===//
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
/// This file contains the implementation of the ioHandlerExample class.
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
#include "example.h"

#include <algorithm>
#include <iostream>
#include <random>

#include "log.h"

/// Template item structure.
/// This map is *copied* into instances of `ioHandlerExample`.
static const std::unordered_map<std::string, shmi::variant_value> items_template {
	// var 0..3 are incremented by simthread
	{"var0", { 0 } },				// VARINT
	{"var1", { 0 } },				// VARINT
	{"var2", { 0.0 } },				// VARFLOAT
	{"var3", { 0.0 } },				// VARFLOAT
	// var 4..9 are left alone (storage variables)
	{"var4", { 0 } },				// VARINT
	{"var5", { 0 } },				// VARINT
	{"var6", { 0 } },				// VARINT
	{"var7", { "String 1" } },		// VARSTRING
	{"var8", { "String 2" } },		// VARSTRING
	{"var9", { false } }			// VARBOOL
};

ioHandlerExample::ioHandlerExample(const ioHandlerParam * const handler)
	: ioHandlerBase(handler)
	, methodHandler_(handler)
	, items_(items_template)
{

	// Start the threads
	thrSimulation_ = std::thread{ &ioHandlerExample::thread_proc_simulation, this };
	thrMonitor_ = std::thread{ &ioHandlerExample::thread_proc_monitor, this };
}

ioHandlerExample::~ioHandlerExample() {
	log(IOLOG_INFO) << "Closing" << std::endl;
	// Make sure all threads are dead when the ioHandler is destroyed
	terminate();
}

void ioHandlerExample::terminate() {
	{
		// Aquire the lock to make sure we're not racing the threads.
		// If we don't do this, we may run into a situation where we call
		// notify_all() without any thread waiting. Now a thread waits for a signal.
		// However there will never be one so the thread may wait indefinitely.
		std::lock_guard<std::mutex> lock(mutex_);
		// Set the _terminate flag first so it's not missed
		terminate_ = true;
		// Wake the threads
		condSimWakeup_.notify_all();
		condMonitorWakeup_.notify_all();
	}


	// Join the threads
	thrSimulation_.join();
	thrMonitor_.join();
	// At this point, all threads are dead
}

void ioHandlerExample::thread_proc_simulation() {
	// Get a unique_lock to make sure nobody interferes with our work
	std::unique_lock<std::mutex> lock(mutex_);
	// Get the current time (once).
	timestamp_type next_tick_timestamp = clock_type::now();
	std::mt19937_64 rng;
	std::locale locale;

	while ( !terminate_ ) {
		// Iterate over all items
		for ( auto & item_value_pair : items_ ) {
			// Skip everything that is not var0, var1, var2, var3
			if ( item_value_pair.first.length() != 4 || (item_value_pair.first[3] < '0' || item_value_pair.first[3] > '3') ) {
				continue;
			}
			switch ( item_value_pair.second.type() ) {
			case shmi::variant_value::kInt: {
				auto & ival = item_value_pair.second.getRefInt();
				// Ints are incremented by one and set to 0 if they exceed 100.
				ival++;
				if ( ival > 100 ) {
					ival = 0;
				}
				break;
			}
			case shmi::variant_value::kUInt: {
				auto & uival = item_value_pair.second.getRefUInt();
				// Unsigned ints are incremented by one and set to 0 if they exceed 100.
				uival++;
				if ( uival > 100 ) {
					uival = 0;
				}
				break;
			}
			case VARFLOAT: {
				auto & dblval = item_value_pair.second.getRefDouble();
				// Floating-point values are incremented by 0.3 and set to 0 if they exceed 100.
				dblval += 0.3;
				if ( dblval > 100 )
					dblval = 0;
				break;
			}
			case VARBOOL:
				// Booleans are flipped.
				item_value_pair.second.getRefBool() = !item_value_pair.second.getRefBool();
				break;
			case VARSTRING:	{
				// Randomly switch the case of characters in the string.
				for ( char & c : item_value_pair.second.getRefString() ) {
					if ( rng() % 2 == 0 ) {
						c = std::toupper(c, locale);
					} else {
						c = std::tolower(c, locale);
					}
				}
				break;
			}
			default:
				// Nothing to do here
				break;
			} // switch ( item_value_pair.second.type )
		} // for ( auto & item_value_pair : items_ )

		// Instead of getting the current time again (which is slow), we just increment the last timestamp by 100ms.
		// This also means that we're not drifting (the computation above requires some time) and in case of system lag,
		// we're going to catch up automatically.
		next_tick_timestamp += std::chrono::milliseconds(100);
		condSimWakeup_.wait_until(lock, next_tick_timestamp);
	} // while ( !_terminate )
}
void ioHandlerExample::thread_proc_monitor() {
	// Get a unique_lock to make sure nobody interferes with our work
	std::unique_lock<std::mutex> lock(mutex_);
	// Get the current time (once).
	timestamp_type next_tick_timestamp = clock_type::now();

	while ( !terminate_ ) {
		// The timestamp of the next tick for the upcoming iteration is going to be stored here.
		timestamp_type min_next_tick_timestamp;
		// Iterate over all SubscriptionGroups
		for ( auto & pair : subscriptionGroups_ ) {
			// Does this SubscriptionGroup require a tick?
			if ( next_tick_timestamp >= pair.second.get_next_tick() )
				pair.second.do_tick();

			// Compute the next time we need to wake up
			// If the timestamp compares to timestamp_type() then this is the first iteration.
			if ( min_next_tick_timestamp == timestamp_type() )
				min_next_tick_timestamp = pair.second.get_next_tick();
			else
				min_next_tick_timestamp = std::min(min_next_tick_timestamp, pair.second.get_next_tick());
		} // for ( auto & pair : subscriptionGroups_ )

		if ( subscriptionGroups_.empty() ) {
			// If there is no SubscriptionGroup, wait until we've been woken up and check again.
			condMonitorWakeup_.wait(lock);
		} else {
			// Wait until there's work to be done.
			// Note that we're not drifting and are going to catch up with lag since we're using absolute time to sleep.
			next_tick_timestamp = min_next_tick_timestamp;
			condMonitorWakeup_.wait_until(lock, next_tick_timestamp);
		} // if ( subscriptionGroups_.empty() )
	} // while ( !terminate_ )
}

SubscriptionGroup & ioHandlerExample::get_subscription_group(std::chrono::milliseconds interval) {
	auto iter = subscriptionGroups_.find(interval);
	if ( iter != subscriptionGroups_.end() )
		return iter->second;
	// SubscriptionGroup does not exist? Create it.
	return subscriptionGroups_.emplace(interval, SubscriptionGroup(*this, std::chrono::duration_cast<duration_type>(std::chrono::milliseconds(interval)))).first->second;
}

shmi::variant_value & ioHandlerExample::get_item_value(const std::string & item_name) {
	auto iter = items_.find(item_name);
	if ( iter != items_.end() )
		return iter->second;
	// Item does not exist? Throw exception.
	// This is a private/internal function. Only call it if you know what you're doing!
	throw std::invalid_argument("No item named \"" + item_name + "\" found");
}

int ioHandlerExample::subscribe(std::chrono::milliseconds interval, const std::vector<std::string> & symbols, const std::vector<int*> & types) {
	// Get a lock to make sure nobody interferes with our work
	std::lock_guard<std::mutex> lock(mutex_);
	SubscriptionGroup & subscription_group = get_subscription_group(interval);

	for ( std::size_t i = 0; i < symbols.size(); i++ ) {
		auto iter = items_.find(symbols[i]);
		if ( iter != items_.end() ) {
			subscription_group.add_item(symbols[i]);
			*types[i] = iter->second.type();
		} else {
			log(IOLOG_ERR) << "No item named \"" << symbols[i] << "\" found." << std::endl;
			*types[i] = VAR_ERR;
		}
	} // for ( std::size_t i = 0; i < symbols.size(); i++ )
	// Wake the monitoring thread as there might be new subscription groups and/or items.
	condMonitorWakeup_.notify_all();
	return 0;
}

int ioHandlerExample::subscribe(std::chrono::milliseconds interval, const char *const * symbols, int * types, std::size_t numSymbols) {
	// Get a lock to make sure nobody interferes with our work
	std::lock_guard<std::mutex> lock(mutex_);
	SubscriptionGroup & subscription_group = get_subscription_group(interval);

	for ( std::size_t i = 0; i < numSymbols; i++ ) {
		auto iter = items_.find(symbols[i]);
		if ( iter != items_.end() ) {
			subscription_group.add_item(symbols[i]);
			types[i] = iter->second.type();
		} else {
			log(IOLOG_ERR) << "No item named \"" << symbols[i] << "\" found." << std::endl;
			types[i] = VAR_ERR;
		}
	} // for ( std::size_t i = 0; i < numSymbols; i++ )
	// Wake the monitoring thread as there might be new subscription groups and/or items.
	condMonitorWakeup_.notify_all();
	return 0;
}

int ioHandlerExample::unsubscribe(const std::vector<std::string> & symbols) {
	// Get a lock to make sure nobody interferes with our work
	std::lock_guard<std::mutex> lock(mutex_);

	for ( const std::string & symbol : symbols ) {
		auto iter = subscriptionGroups_.begin();
		while ( iter != subscriptionGroups_.end() ) {
			iter->second.remove_item(symbol);
			// If the subscription group is empty, remove it
			if ( iter->second.empty() ) {
				subscriptionGroups_.erase(iter++);
			} else {
				iter++;
			}
		} // while ( iter != _subscriptionGroups.end() )
	} // for ( const std::string & symbol : symbols )
	return 0;
}

int ioHandlerExample::unsubscribe(const char *const * symbols, std::size_t numSymbols) {
	// Get a lock to make sure nobody interferes with our work
	std::lock_guard<std::mutex> lock(mutex_);

	for ( std::size_t i = 0; i < numSymbols; i++ ) {
		auto iter = subscriptionGroups_.begin();
		while ( iter != subscriptionGroups_.end() ) {
			iter->second.remove_item(symbols[i]);
			// If the subscription group is empty, remove it
			if ( iter->second.empty() ) {
				subscriptionGroups_.erase(iter++);
			} else {
				iter++;
			}
		} // while ( iter != subscriptionGroups_.end() )
	} // for ( std::size_t i = 0; i < numSymbols; i++ )
	return 0;
}

int ioHandlerExample::write(const std::string & symbol, const ioDataValue & value) {
	// Get a lock to make sure nobody interferes with our work
	std::lock_guard<std::mutex> lock(mutex_);

	auto iter = items_.find(symbol);
	if ( iter != items_.end() ) {
		try {
			switch ( value.varType ) {
			case VARINT:
				iter->second = value.iVal;
				break;
			case VARUINT:
				iter->second = value.uiVal;
				break;
			case VARBOOL:
				iter->second = !!value.iVal;
				break;
			case VARFLOAT:
				iter->second = value.dVal;
				break;
			case VARSTRING:
				iter->second = value.sVal;
				break;
			default:
				return -1;
			}
			return 0;
		} catch ( const std::invalid_argument & ) {
		}
		// Value conversion error? Fail.
	}
	// Item does not exist? Fail.
	return -1;
}

int ioHandlerExample::read(const std::string & symbol) {
	// Get a lock to make sure nobody interferes with our work
	std::lock_guard<std::mutex> lock(mutex_);

	auto iter = items_.find(symbol);
	if ( iter != items_.end() ) {
		send_item_value(symbol,	iter->second);
		return 0;
	} // if ( iter != _items.end() )
	return -1;
}

int ioHandlerExample::method(const void * context, const std::string & methodName, const std::string & methodArguments) {
	return methodHandler_.call(context, methodName, methodArguments) ? 0 : 1;
}
