#pragma once

#include <cstdarg>
#include <filesystem>
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

namespace Util
{

	class Logger {
	public:
		enum ErrorType {
			NONE,
			ERR_WARNING,
			ERR_ERROR,
			ERR_SCRIPT,
			ERR_SHADER,
		};

		static constexpr const char* error_type_string(ErrorType p_type) {
			switch (p_type) {
			case ERR_ERROR:
				return "ERROR";
			case ERR_WARNING:
				return "WARNING";
			case ERR_SCRIPT:
				return "SCRIPT ERROR";
			case ERR_SHADER:
				return "SHADER ERROR";
			}
			return "UNKNOWN ERROR";
		}

		static constexpr const char* error_type_indent(ErrorType p_type) {
			switch (p_type) {
			case ERR_ERROR:
				return "   ";
			case ERR_WARNING:
				return "     ";
			case ERR_SCRIPT:
				return "          ";
			case ERR_SHADER:
				return "          ";
			}
			return "           ";
		}

		static void set_flush_stdout_on_print(bool value);

		virtual void logv(const char* p_format, va_list p_list, ErrorType p_type = ErrorType::NONE) = 0;
		virtual void log_error(const char* p_function, const char* p_file, int p_line, const char* p_code, const char* p_rationale, bool p_editor_notify = false, ErrorType p_type = ERR_ERROR);

		void logf(ErrorType p_type, const char* p_format, ...);
		void logf_error(const char* p_format, ...);
		void logf_warn(const char* p_format, ...);
		void logf_info(const char* p_format, ...);

		virtual ~Logger() {}

	protected:
		bool should_log(ErrorType p_type);

		static inline bool _flush_stdout_on_print = true;
	};

	/**
	 * Writes messages to stdout/stderr.
	 */
	class StdLogger : public Logger {
	public:
		virtual void logv(const char* p_format, va_list p_list, ErrorType p_type) override;
		virtual ~StdLogger() {}
	};

	/**
	 * Writes messages to stdout/stderr.
	 */
	class StdSpdLogger : public Logger {
		std::shared_ptr<spdlog::logger> m_logger;
	public:
		StdSpdLogger();
		virtual void logv(const char* p_format, va_list p_list, ErrorType p_type) override;
		virtual ~StdSpdLogger() {};
	};


	/**
	 * Writes messages to the specified file. If the file already exists, creates a copy (backup)
	 * of it with timestamp appended to the file name. Maximum number of backups is configurable.
	 * When maximum is reached, the oldest backups are erased. With the maximum being equal to 1,
	 * it acts as a simple file logger.
	 */
	class RotatedFileLogger : public StdLogger {
		fs::path base_path;
		int max_files;

		std::shared_ptr<spdlog::logger> m_logger;
		//void clear_old_backups();
		//void rotate_file();

	public:
		explicit RotatedFileLogger(const fs::path& p_base_path, int p_max_files = 10, bool rotate_on_open = false);

		virtual void logv(const char* p_format, va_list p_list, ErrorType p_type) override;
	};

	class CompositeLogger : public Logger {
		std::vector<Logger*> loggers;

	public:
		explicit CompositeLogger(const std::vector<Logger*>& p_loggers);

		virtual void logv(const char* p_format, va_list p_list, ErrorType p_type) override;
		virtual void log_error(const char* p_function, const char* p_file, int p_line, const char* p_code, const char* p_rationale, bool p_editor_notify, ErrorType p_type = ERR_ERROR) override;

		void add_logger(Logger* p_logger);

		virtual ~CompositeLogger();
	};

	void set_logger_iface(Logger* iface);
	Logger* get_logger_iface();
}

#define LOGE(...) do { ((::Util::StdSpdLogger*)::Util::get_logger_iface())->logf_error(__VA_ARGS__);} while(0)
#define LOGW(...) do { ((::Util::StdSpdLogger*)::Util::get_logger_iface())->logf_warn(__VA_ARGS__);} while(0)
#define LOGI(...) do { ((::Util::StdSpdLogger*)::Util::get_logger_iface())->logf_info(__VA_ARGS__);} while(0)
#define LOG(...) do { ((::Util::StdSpdLogger*)::Util::get_logger_iface())->logf(__VA_ARGS__);} while(0)

#define LOGF(...) do { ((::Util::StdSpdLogger*)::Util::get_logger_iface())->log_error(__VA_ARGS__);} while(0)


