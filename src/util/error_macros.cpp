/**************************************************************************/
/*  error_macros.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "error_macros.h"

#include "logger.h"

#include "_mutex.h"
#include <cstdio>

static ErrorHandlerList *error_handler_list = nullptr;
static thread_local bool is_printing_error = false;

static void _err_print_fallback(const char *p_function, const char *p_file, int p_line, const char *p_error_details, ErrorHandlerType p_type, bool p_reentrance) {
	if (p_reentrance) {
		fprintf(stderr, "While attempting to print an error, another error was printed:\n");
	}

	fprintf(stderr, "%s: %s\n", _error_handler_type_string(p_type), p_error_details);

	if (p_function && p_file) {
		fprintf(stderr, "   at: %s (%s:%i)\n", p_function, p_file, p_line);
	}
}

void add_error_handler(ErrorHandlerList *p_handler) {
	// If p_handler is already in error_handler_list
	// we'd better remove it first then we can add it.
	// This prevent cyclic redundancy.
	remove_error_handler(p_handler);

	_global_lock();

	p_handler->next = error_handler_list;
	error_handler_list = p_handler;

	_global_unlock();
}

void remove_error_handler(const ErrorHandlerList *p_handler) {
	_global_lock();

	ErrorHandlerList *prev = nullptr;
	ErrorHandlerList *l = error_handler_list;

	while (l) {
		if (l == p_handler) {
			if (prev) {
				prev->next = l->next;
			} else {
				error_handler_list = l->next;
			}
			break;
		}
		prev = l;
		l = l->next;
	}

	_global_unlock();
}

// Errors without messages.
void _err_print_error(const char *p_function, const char *p_file, int p_line, const char *p_error, bool p_editor_notify, ErrorHandlerType p_type) {
	_err_print_error(p_function, p_file, p_line, p_error, "", p_editor_notify, p_type);
}

void _err_print_error(const char *p_function, const char *p_file, int p_line, const std::string &p_error, bool p_editor_notify, ErrorHandlerType p_type) {
	_err_print_error(p_function, p_file, p_line, p_error.c_str(), "", p_editor_notify, p_type);
}

// Main error printing function.
void _err_print_error(const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, bool p_editor_notify, ErrorHandlerType p_type) {
	if (is_printing_error) {
		// Fallback if we're already printing an error, to prevent infinite recursion.
		const char *err_details = (p_message && *p_message) ? p_message : p_error;
		_err_print_fallback(p_function, p_file, p_line, err_details, p_type, true);
		return;
	}

	is_printing_error = true;

	LOGF(p_function, p_file, p_line, p_error, p_message, p_editor_notify, (Util::Logger::ErrorType)p_type);

	_global_lock();

	ErrorHandlerList *l = error_handler_list;
	while (l) {
		l->errfunc(l->userdata, p_function, p_file, p_line, p_error, p_message, p_editor_notify, p_type);
		l = l->next;
	}

	_global_unlock();

	is_printing_error = false;
}

// For printing errors when we may crash at any point, so we must flush ASAP a lot of lines
// but we don't want to make it noisy by printing lots of file & line info (because it's already
// been printing by a preceding _err_print_error).
void _err_print_error_asap(const std::string &p_error, ErrorHandlerType p_type) {
	const char *err_details = p_error.c_str();

	if (is_printing_error) {
		// Fallback if we're already printing an error, to prevent infinite recursion.
		_err_print_fallback(nullptr, nullptr, 0, err_details, p_type, true);
		return;
	}

	is_printing_error = true;

	LOGE("%s: %s\n", _error_handler_type_string(p_type), err_details);

	_global_lock();

	ErrorHandlerList *l = error_handler_list;
	while (l) {
		l->errfunc(l->userdata, "", "", 0, err_details, "", false, p_type);
		l = l->next;
	}

	_global_unlock();

	is_printing_error = false;
}

//TODO:
// Errors with message. (All combinations of p_error and p_message as std::string or char*.)
//void _err_print_error(const char *p_function, const char *p_file, int p_line, const std::string &p_error, const char *p_message, bool p_editor_notify, ErrorHandlerType p_type) {
//	_err_print_error(p_function, p_file, p_line, p_error, p_message, p_editor_notify, p_type);
//}

//void _err_print_error(const char *p_function, const char *p_file, int p_line, const char *p_error, const std::string &p_message, bool p_editor_notify, ErrorHandlerType p_type) {
//	_err_print_error(p_function, p_file, p_line, p_error, p_message, p_editor_notify, p_type);
//}
//
void _err_print_error(const char *p_function, const char *p_file, int p_line, const std::string &p_error, const std::string &p_message, bool p_editor_notify, ErrorHandlerType p_type) {
	_err_print_error(p_function, p_file, p_line, p_error.c_str(), p_message.c_str(), p_editor_notify, p_type);
}

// Index errors. (All combinations of p_message as std::string or char*.)
void _err_print_index_error(const char *p_function, const char *p_file, int p_line, int64_t p_index, int64_t p_size, const char *p_index_str, const char *p_size_str, const char *p_message, bool p_editor_notify, bool p_fatal) {
	std::string fstr(p_fatal ? "FATAL: " : "");
	std::string err(fstr + "Index " + p_index_str + " = " + std::to_string(p_index) + " is out of bounds (" + p_size_str + " = " + std::to_string(p_size) + ").");
	_err_print_error(p_function, p_file, p_line, err, p_message, p_editor_notify, ERR_HANDLER_ERROR);
}

//void _err_print_index_error(const char *p_function, const char *p_file, int p_line, int64_t p_index, int64_t p_size, const char *p_index_str, const char *p_size_str, const std::string &p_message, bool p_editor_notify, bool p_fatal) {
//	_err_print_index_error(p_function, p_file, p_line, p_index, p_size, p_index_str, p_size_str, p_message, p_editor_notify, p_fatal);
//}

void _err_flush_stdout() {
	fflush(stdout);
}

//// Prevent error spam by limiting the warnings to a certain frequency.
//void _physics_interpolation_warning(const char *p_function, const char *p_file, int p_line, ObjectID p_id, const char *p_warn_string) {
//#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
//	const uint32_t warn_max = 2048;
//	const uint32_t warn_timeout_seconds = 15;
//
//	static uint32_t warn_count = warn_max;
//	static uint32_t warn_timeout = warn_timeout_seconds;
//
//	uint32_t time_now = UINT32_MAX;
//
//	if (warn_count) {
//		warn_count--;
//	}
//
//	if (!warn_count) {
//		time_now = OS::get_singleton()->get_ticks_msec() / 1000;
//	}
//
//	if ((warn_count == 0) && (time_now >= warn_timeout)) {
//		warn_count = warn_max;
//		warn_timeout = time_now + warn_timeout_seconds;
//
//		if (GLOBAL_GET("debug/settings/physics_interpolation/enable_warnings")) {
//			// UINT64_MAX means unused.
//			if (p_id.operator uint64_t() == UINT64_MAX) {
//				_err_print_error(p_function, p_file, p_line, "[Physics interpolation] " + std::string(p_warn_string) + " (possibly benign).", false, ERR_HANDLER_WARNING);
//			} else {
//				std::string node_name;
//				if (p_id.is_valid()) {
//					Node *node = ObjectDB::get_instance<Node>(p_id);
//					if (node && node->is_inside_tree()) {
//						node_name = "\"" + std::string(node->get_path()) + "\"";
//					} else {
//						node_name = "\"unknown\"";
//					}
//				}
//
//				_err_print_error(p_function, p_file, p_line, "[Physics interpolation] " + std::string(p_warn_string) + ": " + node_name + " (possibly benign).", false, ERR_HANDLER_WARNING);
//			}
//		}
//	}
//#endif
//}
