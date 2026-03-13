#include "logger.h"
#include <stdio.h>
#include "spdlog/sinks/rotating_file_sink.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include "libassert/assert.hpp"

namespace Util
{
	void Logger::log_error(const char* p_function, const char* p_file, int p_line, const char* p_code, const char* p_rationale, bool p_editor_notify, ErrorType p_type)
	{
		if (!should_log(p_type)) {
			return;
		}

		const char* err_type = error_type_string(p_type);

		const char* err_details;
		if (p_rationale && *p_rationale) {
			err_details = p_rationale;
		}
		else {
			err_details = p_code;
		}

		logf(p_type, "%s: %s\n", err_type, err_details);
		logf(p_type, "   at: %s (%s:%i)\n", p_function, p_file, p_line);
	}

	void Logger::logf(ErrorType p_type, const char* p_format, ...) {
		if (!should_log(p_type)) {
			return;
		}

		va_list argp;
		va_start(argp, p_format);

		logv(p_format, argp, p_type);

		va_end(argp);
	}

	void Logger::logf_error(const char* p_format, ...) {
		if (!should_log(ErrorType::ERR_ERROR)) {
			return;
		}

		va_list argp;
		va_start(argp, p_format);

		logv(p_format, argp, ErrorType::ERR_ERROR);

		va_end(argp);
	}

	void Logger::logf_warn(const char* p_format, ...)
	{
		if (!should_log(ErrorType::ERR_WARNING)) {
			return;
		}

		va_list argp;
		va_start(argp, p_format);

		logv(p_format, argp, ErrorType::ERR_WARNING);

		va_end(argp);
	}

	void Logger::logf_info(const char* p_format, ...)
	{
		if (!should_log(ErrorType::NONE)) {
			return;
		}

		va_list argp;
		va_start(argp, p_format);

		logv(p_format, argp, ErrorType::NONE);

		va_end(argp);
	}

	bool Logger::should_log(ErrorType p_type) {
		// SHAN: TODO: add global controls 
		return (true /*|| CoreGlobals::print_error_enabled) && (p_err || CoreGlobals::print_line_enabled*/);
	}

	void Logger::set_flush_stdout_on_print(bool value) {
		_flush_stdout_on_print = value;
	}

	void StdLogger::logv(const char* p_format, va_list p_list, ErrorType p_type)
	{
		if (!should_log(p_type)) {
			return;
		}

		switch (p_type)
		{
		case Util::Logger::NONE:
			vprintf(p_format, p_list);
			if (_flush_stdout_on_print) {
				// Don't always flush when printing stdout to avoid performance
				// issues when `print()` is spammed in release builds.
				fflush(stdout);
			}
			break;
		case Util::Logger::ERR_ERROR:
		case Util::Logger::ERR_WARNING:
		case Util::Logger::ERR_SCRIPT:
		case Util::Logger::ERR_SHADER:
		default:
			vfprintf(stderr, p_format, p_list);
			break;
		}
	}

	StdSpdLogger::StdSpdLogger()
	{
		m_logger = spdlog::stdout_color_mt("spd_logger");
		m_logger->set_pattern("[%^%l%$] %v");
	}

	void StdSpdLogger::logv(const char* p_format, va_list p_list, ErrorType p_type)
	{
		// 1. Standard spdlog 'should_log' check
		if (!should_log(p_type)) {
			return;
		}

		// 2. Format the va_list into a string
		// Note: spdlog/fmt doesn't take va_list directly, so we use vsnprintf
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), p_format, p_list);

		switch (p_type)
		{
		case Util::Logger::NONE:
		{
			m_logger->info("{}", buffer);

			// 4. Handle flushing manually if needed
			if (_flush_stdout_on_print) {
				m_logger->flush();
			}
			break;
		}
		case Util::Logger::ERR_WARNING:
			m_logger->warn("{}", buffer);
			break;
		case Util::Logger::ERR_ERROR:
		case Util::Logger::ERR_SCRIPT:
		case Util::Logger::ERR_SHADER:
			m_logger->error("{}", buffer);
			break;
		default:
			m_logger->info("{}", buffer);
			break;
		}
	}

	RotatedFileLogger::RotatedFileLogger(const fs::path& p_base_path, int p_max_files, bool rotate_on_open) :
		base_path(p_base_path),
		max_files(p_max_files > 0 ? p_max_files : 1)
	{
		auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(base_path.string(), 1024 * 1024 * 5, 3, rotate_on_open);

		// Construct the member variable
		m_logger = std::make_shared<spdlog::logger>("spd_rotated_file_logger", sink);
		m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

		// no need because we are storing it ourselves.
		//spdlog::register_logger(m_logger);
	}

	void RotatedFileLogger::logv(const char* p_format, va_list p_list, ErrorType p_type)
	{
		// 1. Level Check (Replaces should_log)
		if (!should_log(p_type)) {
			return;
		}

		auto level = spdlog::level::info;
		switch (p_type)
		{
		case Util::Logger::NONE:
			level = spdlog::level::info;
			break;
		case Util::Logger::ERR_WARNING:
			level = spdlog::level::warn;
			break;
		case Util::Logger::ERR_ERROR:
		case Util::Logger::ERR_SCRIPT:
		case Util::Logger::ERR_SHADER:
			level = spdlog::level::err;
			break;
		default:
			level = spdlog::level::info;
			break;
		}

		// 2. Expand va_list into a formatted string
		// We use vsnprintf once to get the size, then format into a string.
		va_list list_copy;
		va_copy(list_copy, p_list);

		int len = vsnprintf(nullptr, 0, p_format, list_copy);
		va_end(list_copy);

		if (len > 0) {
			std::string buf(len, '\0');
			vsnprintf(&buf[0], len + 1, p_format, p_list);

			// 3. Log the message
			// spdlog sinks for files do not process ANSI codes by default, 
			// so the Regex stripping is likely unnecessary unless your strings 
			// contain literal escape characters you want gone.
			m_logger->log(level, "{}", buf);

			// 4. Handle Flushing
			if (p_type >= ErrorType::ERR_ERROR || _flush_stdout_on_print) {
				m_logger->flush();
			}
		}
	}


	CompositeLogger::CompositeLogger(const std::vector<Logger*>& p_loggers) :
		loggers(p_loggers) {
	}

	void CompositeLogger::logv(const char* p_format, va_list p_list, ErrorType p_type) {
		if (!should_log(p_type)) {
			return;
		}

		for (int i = 0; i < loggers.size(); ++i) {
			va_list list_copy;
			va_copy(list_copy, p_list);
			loggers[i]->logv(p_format, list_copy, p_type);
			va_end(list_copy);
		}
	}

	void CompositeLogger::log_error(const char* p_function, const char* p_file, int p_line, const char* p_code, const char* p_rationale, bool p_editor_notify, ErrorType p_type) {
		if (!should_log(p_type)) {
			return;
		}

		for (int i = 0; i < loggers.size(); ++i) {
			loggers[i]->log_error(p_function, p_file, p_line, p_code, p_rationale, p_editor_notify, p_type);
		}
	}

	void CompositeLogger::add_logger(Logger* p_logger) {
		loggers.push_back(p_logger);
	}

	CompositeLogger::~CompositeLogger() {
		for (int i = 0; i < loggers.size(); ++i) {
			//SHAN: TODO: change if custom allocator 
			delete loggers[i];
		}
	}

	static Logger* logger_iface = nullptr;

	void set_logger_iface(Logger* iface)
	{
		logger_iface = iface;
	}

	Logger* get_logger_iface()
	{
		DEBUG_ASSERT(logger_iface);
		return logger_iface;
	}
}