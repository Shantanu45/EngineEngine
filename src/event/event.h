#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <utility>
#include "libassert/assert.hpp"
#include "util/compile_time_hash.h"

#define EE_MANAGER_REGISTER(clazz, member, event) \
	EE_EVENT_MANAGER()->register_handler<clazz, event, &clazz::member>(this)
#define EE_MANAGER_REGISTER_LATCH(clazz, up_event, down_event, event) \
	EE_EVENT_MANAGER()->register_latch_handler<clazz, event, &clazz::up_event, &clazz::down_event>(this)

//SHAN: TODO: make sure that change to unordered map works

namespace EE
{
class Event;

template <typename Return, typename T, typename EventType, Return (T::*callback)(const EventType &e)>
Return member_function_invoker(void *object, const Event &e)
{
	return (static_cast<T *>(object)->*callback)(static_cast<const EventType &>(e));
}

#define GRANITE_EVENT_TYPE_HASH(x) ::Util::compile_time_fnv1(#x)
using EventType = uint64_t;

#define GRANITE_EVENT_TYPE_DECL(x) \
enum class EventTypeWrapper : ::Granite::EventType { \
	type_id = GRANITE_EVENT_TYPE_HASH(x) \
}; \
static inline constexpr ::Granite::EventType get_type_id() { \
	return ::Granite::EventType(EventTypeWrapper::type_id); \
}

class Event
{
public:
	virtual ~Event() = default;

	Event() = default;

	// Doesn't have to be set unless type information is going to be lost.
	// E.g. we're storing the Event some place and we have type-erasure.
	// Having this set helps us recover type information for dispatch.
	explicit Event(EventType type_)
		: type(type_)
	{
	}

	EventType get_type_id() const
	{
		return type;
	}

	void set_cookie(uint64_t cookie_)
	{
		cookie = cookie_;
	}

	uint64_t get_cookie() const
	{
		return cookie;
	}

private:
	EventType type = 0;
	uint64_t cookie = 0;
};

class EventManager;

class EventHandler
{
public:
	EventHandler(const EventHandler &) = delete;
	void operator=(const EventHandler &) = delete;
	EventHandler() = default;
	~EventHandler();

	void add_manager_reference(EventManager *manager);
	void release_manager_reference();

private:
	EventManager *event_manager = nullptr;
	uint32_t event_manager_ref_count = 0;
};

class EventManagerInterface
{
public:
	virtual ~EventManagerInterface() = default;
};


class EventManager final : public EventManagerInterface
{
public:
	template<typename T, typename... P>
	void enqueue(P&&... p)
	{
		static constexpr auto type = T::get_type_id();
		auto &l = events[type];

		auto ptr = std::unique_ptr<Event>(new T(std::forward<P>(p)...));
		l.queued_events.emplace_back(std::move(ptr));
	}

	template<typename T, typename... P>
	uint64_t enqueue_latched(P&&... p)
	{
		static constexpr auto type = T::get_type_id();
		auto &l = latched_events[type];
		auto ptr = std::unique_ptr<Event>(new T(std::forward<P>(p)...));
		uint64_t cookie = ++cookie_counter;
		ptr->set_cookie(cookie);

		if (l.enqueueing)
			throw std::logic_error("Cannot enqueue more latched events while handling events.");
		l.enqueueing = true;

		auto *event = ptr.get();
		l.queued_events.emplace_back(std::move(ptr));
		dispatch_up_event(l, *event);
		l.enqueueing = false;
		return cookie;
	}

	void dequeue_latched(uint64_t cookie);
	void dequeue_all_latched(EventType type);

	template<typename T>
	void dispatch_inline(const T &t)
	{
		static constexpr auto type = T::get_type_id();
		auto &l = events[type];
		dispatch_event(l.handlers, t);
	}

	void dispatch_inline(const Event &e)
	{
		DEBUG_ASSERT(e.get_type_id() != 0);
		auto &l = events[e.get_type_id()];
		dispatch_event(l.handlers, e);
	}

	void dispatch();

	template<typename T, typename EventType, bool (T::*mem_fn)(const EventType &)>
	void register_handler(T *handler)
	{
		handler->add_manager_reference(this);
		static constexpr auto type_id = EventType::get_type_id();
		auto &l = events[type_id];
		if (l.dispatching)
			l.recursive_handlers.push_back({ member_function_invoker<bool, T, EventType, mem_fn>, handler, handler });
		else
			l.handlers.push_back({ member_function_invoker<bool, T, EventType, mem_fn>, handler, handler });
	}

	void unregister_handler(EventHandler *handler);

	template<typename T, typename EventType, void (T::*up_fn)(const EventType &), void (T::*down_fn)(const EventType &)>
	void register_latch_handler(T *handler)
	{
		handler->add_manager_reference(this);
		LatchHandler h{
			member_function_invoker<void, T, EventType, up_fn>,
			member_function_invoker<void, T, EventType, down_fn>,
			handler, handler };

		static constexpr auto type_id = EventType::get_type_id();
		auto &levents = latched_events[type_id];
		dispatch_up_events(levents.queued_events, h);

		auto &l = latched_events[type_id];
		if (l.dispatching)
			l.recursive_handlers.push_back(h);
		else
			l.handlers.push_back(h);
	}

	void unregister_latch_handler(EventHandler *handler);

	~EventManager();

private:
	struct Handler
	{
		bool (*mem_fn)(void *object, const Event &event);
		void *handler;
		EventHandler *unregister_key;
	};

	struct LatchHandler
	{
		void (*up_fn)(void *object, const Event &event);
		void (*down_fn)(void *object, const Event &event);
		void *handler;
		EventHandler *unregister_key;
	};

	struct EventTypeData
	{
		std::vector<std::unique_ptr<Event>> queued_events;
		std::vector<Handler> handlers;
		std::vector<Handler> recursive_handlers;
		bool enqueueing = false;
		bool dispatching = false;

		void flush_recursive_handlers();
	};

	struct LatchEventTypeData
	{
		std::vector<std::unique_ptr<Event>> queued_events;
		std::vector<LatchHandler> handlers;
		std::vector<LatchHandler> recursive_handlers;
		bool enqueueing = false;
		bool dispatching = false;

		void flush_recursive_handlers();
	};

	void dispatch_event(std::vector<Handler> &handlers, const Event &e);
	void dispatch_up_events(std::vector<std::unique_ptr<Event>> &events, const LatchHandler &handler);
	void dispatch_down_events(std::vector<std::unique_ptr<Event>> &events, const LatchHandler &handler);
	void dispatch_up_event(LatchEventTypeData &event_type, const Event &event);
	void dispatch_down_event(LatchEventTypeData &event_type, const Event &event);

	std::unordered_map<uint64_t, EventTypeData> events;
	std::unordered_map<uint64_t, LatchEventTypeData> latched_events;
	uint64_t cookie_counter = 0;
};
}
