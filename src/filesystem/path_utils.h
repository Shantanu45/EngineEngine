#pragma once
#include <string>
#include <utility>
#include <vector>

namespace FileSystem
{
	namespace Path
	{
		std::string join(const std::string& base, const std::string& path);
		std::string basedir(const std::string& path);
		std::string basename(const std::string& path);
		std::pair<std::string, std::string> split(const std::string& path);
		std::string relpath(const std::string& base, const std::string& path);
		std::string ext(const std::string& path);
		std::pair<std::string, std::string> protocol_split(const std::string& path);
		bool is_abspath(const std::string& path);
		bool is_root_path(const std::string& path);
		std::string canonicalize_path(const std::string& path);
		std::string enforce_protocol(const std::string& path);
		std::string get_executable_path();

#ifdef _WIN32
		std::string to_utf8(const wchar_t* wstr, size_t len);
		std::wstring to_utf16(const char* str, size_t len);
		std::string to_utf8(const std::wstring& wstr);
		std::wstring to_utf16(const std::string& str);
#endif

		inline std::vector<std::string> split(const std::string& str, const char* delim, bool allow_empty)
		{
			if (str.empty())
				return {};
			std::vector<std::string> ret;

			size_t start_index = 0;
			size_t index = 0;
			while ((index = str.find_first_of(delim, start_index)) != std::string::npos)
			{
				if (allow_empty || index > start_index)
					ret.push_back(str.substr(start_index, index - start_index));
				start_index = index + 1;

				if (allow_empty && (index == str.size() - 1))
					ret.emplace_back();
			}

			if (start_index < str.size())
				ret.push_back(str.substr(start_index));
			return ret;
		}

		inline std::vector<std::string> split(const std::string& str, const char* delim)
		{
			return split(str, delim, true);
		}

		inline std::vector<std::string> split_no_empty(const std::string& str, const char* delim)
		{
			return split(str, delim, false);
		}

		inline std::string strip_whitespace(const std::string& str)
		{
			std::string ret;
			auto index = str.find_first_not_of(" \t");
			if (index == std::string::npos)
				return "";
			ret = str.substr(index, std::string::npos);
			index = ret.find_last_not_of(" \t");
			if (index != std::string::npos)
				return ret.substr(0, index + 1);
			else
				return ret;
		}
	}
}