/*****************************************************************//**
 * \file   event.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   April 2026
 *********************************************************************/
#include "event.h"
#include <algorithm>
#include <assert.h>

namespace EE
{

	EventManager::~EventManager()
	{
		dispatch();
		// FIX: release references for normal event handlers so EventHandler objects
		// that outlive the manager don't hold a dangling event_manager pointer
		release_all_normal_handlers();

		for (auto& event_type : latched_events)
		{
			for (auto& handler : event_type.second.handlers)
			{
				dispatch_down_events(event_type.second.queued_events, handler);
				handler.unregister_key->release_manager_reference();
			}
		}
	}

	void EventManager::release_all_normal_handlers()
	{
		for (auto& event_type : events)
		{
			for (auto& handler : event_type.second.handlers)
				handler.unregister_key->release_manager_reference();
			event_type.second.handlers.clear();

			// FIX: also release any handlers that were registered during dispatch
			// and are sitting in recursive_handlers waiting to be flushed
			for (auto& handler : event_type.second.recursive_handlers)
				handler.unregister_key->release_manager_reference();
			event_type.second.recursive_handlers.clear();
		}
	}

	void EventManager::dispatch()
	{
		for (auto& event_type : events)
		{
			auto& handlers = event_type.second.handlers;
			auto& queued_events = event_type.second.queued_events;

			if (queued_events.empty())
				continue;

			// Snapshot and clear the queue before dispatching so that events enqueued
			// by handlers during dispatch are deferred to the next dispatch() call
			// rather than processed mid-iteration (which could invalidate iterators).
			std::vector<std::unique_ptr<Event>> current_events;
			current_events.swap(queued_events);

			event_type.second.dispatching = true;
			auto itr = remove_if(begin(handlers), end(handlers), [&](const Handler& handler) {
				for (auto& event : current_events)
				{
					if (!handler.mem_fn(handler.handler, *event))
					{
						handler.unregister_key->release_manager_reference();
						return true;
					}
				}
				return false;
				});
			handlers.erase(itr, end(handlers));
			event_type.second.dispatching = false;
			event_type.second.flush_recursive_handlers();
		}
	}

	void EventManager::dispatch_event(std::vector<Handler>& handlers, const Event& e)
	{
		auto itr = remove_if(begin(handlers), end(handlers), [&](const Handler& handler) -> bool {
			bool to_remove = !handler.mem_fn(handler.handler, e);
			if (to_remove)
				handler.unregister_key->release_manager_reference();
			return to_remove;
			});

		handlers.erase(itr, end(handlers));
	}

	void EventManager::dispatch_up_events(std::vector<std::unique_ptr<Event>>& up_events, const LatchHandler& handler)
	{
		for (auto& event : up_events)
			handler.up_fn(handler.handler, *event);
	}

	void EventManager::dispatch_down_events(std::vector<std::unique_ptr<Event>>& down_events, const LatchHandler& handler)
	{
		for (auto& event : down_events)
			handler.down_fn(handler.handler, *event);
	}

	void EventManager::LatchEventTypeData::flush_recursive_handlers()
	{
		handlers.insert(end(handlers), begin(recursive_handlers), end(recursive_handlers));
		recursive_handlers.clear();
	}

	void EventManager::EventTypeData::flush_recursive_handlers()
	{
		handlers.insert(end(handlers), begin(recursive_handlers), end(recursive_handlers));
		recursive_handlers.clear();
	}

	void EventManager::dispatch_up_event(LatchEventTypeData& event_type, const Event& event)
	{
		event_type.dispatching = true;
		for (auto& handler : event_type.handlers)
			handler.up_fn(handler.handler, event);
		event_type.flush_recursive_handlers();
		event_type.dispatching = false;
	}

	void EventManager::dispatch_down_event(LatchEventTypeData& event_type, const Event& event)
	{
		event_type.dispatching = true;
		for (auto& handler : event_type.handlers)
			handler.down_fn(handler.handler, event);
		event_type.flush_recursive_handlers();
		event_type.dispatching = false;
	}

	void EventManager::unregister_handler(EventHandler* handler)
	{
		for (auto& event_type : events)
		{
			if (event_type.second.dispatching)
				throw std::logic_error("Unregistering handlers while dispatching events.");

			auto remove_from = [&](std::vector<Handler>& vec)
				{
					auto itr = remove_if(begin(vec), end(vec), [&](const Handler& h) -> bool {
						bool to_remove = h.unregister_key == handler;
						if (to_remove)
							h.unregister_key->release_manager_reference();
						return to_remove;
						});
					vec.erase(itr, end(vec));
				};

			remove_from(event_type.second.handlers);
			// FIX: also remove from recursive_handlers to avoid stale pointers
			remove_from(event_type.second.recursive_handlers);
		}
	}

	void EventManager::unregister_latch_handler(EventHandler* handler)
	{
		for (auto& event_type : latched_events)
		{
			auto remove_from = [&](std::vector<LatchHandler>& vec)
				{
					auto itr = remove_if(begin(vec), end(vec), [&](const LatchHandler& h) -> bool {
						bool to_remove = h.unregister_key == handler;
						if (to_remove)
							h.unregister_key->release_manager_reference();
						return to_remove;
						});
					vec.erase(itr, end(vec));
				};

			remove_from(event_type.second.handlers);
			// FIX: also remove from recursive_handlers
			remove_from(event_type.second.recursive_handlers);
		}
	}

	void EventManager::dequeue_latched(uint64_t cookie)
	{
		// FIX: O(1) lookup using cookie_to_type map instead of scanning all event types
		auto type_itr = cookie_to_type.find(cookie);
		if (type_itr == cookie_to_type.end())
			return;

		EventType type = type_itr->second;
		auto& event_type = latched_events[type];

		if (event_type.enqueueing)
			throw std::logic_error("Dequeueing latched while queueing events.");

		struct EnqueueGuard
		{
			bool& flag;
			explicit EnqueueGuard(bool& f) : flag(f) { flag = true; }
			~EnqueueGuard() { flag = false; }
		} guard(event_type.enqueueing);

		auto& queued_events = event_type.queued_events;

		// FIX: separate find from dispatch from erase - avoids side effects inside remove_if predicate
		auto itr = std::find_if(begin(queued_events), end(queued_events),
			[&](const std::unique_ptr<Event>& event) {
				return event->get_cookie() == cookie;
			});

		if (itr != end(queued_events))
		{
			dispatch_down_event(event_type, **itr);
			queued_events.erase(itr);
			cookie_to_type.erase(cookie);
		}
	}

	void EventManager::dequeue_all_latched(EventType type)
	{
		auto& event_type = latched_events[type];
		if (event_type.enqueueing)
			throw std::logic_error("Dequeueing latched while queueing events.");

		struct EnqueueGuard
		{
			bool& flag;
			explicit EnqueueGuard(bool& f) : flag(f) { flag = true; }
			~EnqueueGuard() { flag = false; }
		} guard(event_type.enqueueing);

		for (auto& event : event_type.queued_events)
		{
			// FIX: clean up cookie map entries as we dequeue
			cookie_to_type.erase(event->get_cookie());
			dispatch_down_event(event_type, *event);
		}
		event_type.queued_events.clear();
	}

	void EventHandler::release_manager_reference()
	{
		assert(event_manager_ref_count > 0);
		assert(event_manager);
		if (--event_manager_ref_count == 0)
			event_manager = nullptr;
	}

	void EventHandler::add_manager_reference(EventManager* manager)
	{
		assert(!event_manager_ref_count || manager == event_manager);
		event_manager = manager;
		event_manager_ref_count++;
	}

	EventHandler::~EventHandler()
	{
		if (event_manager)
			event_manager->unregister_handler(this);
		if (event_manager)
			event_manager->unregister_latch_handler(this);
		assert(event_manager_ref_count == 0 && !event_manager);
	}
}