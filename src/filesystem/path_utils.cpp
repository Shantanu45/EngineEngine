/*****************************************************************//**
 * \file   path_utils.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#define NOMINMAX
#include "path_utils.h"
#include <filesystem>
#include <algorithm>
#include <vector>
#include "util/small_vector.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace FileSystem
{
    namespace Path
    {
        namespace fs = std::filesystem;

        // ------------------------------------------------------------
        // Protocol Handling (filesystem does not handle URI schemes)
        // ------------------------------------------------------------

        std::string enforce_protocol(const std::string& path)
        {
            if (path.empty())
                return {};

            if (path.find("://") == std::string::npos)
                return "file://" + path;

            return path;
        }

        std::pair<std::string, std::string>
            protocol_split(const std::string& path)
        {
            auto pos = path.find("://");
            if (pos == std::string::npos)
                return { "", path };

            return {
                path.substr(0, pos),
                path.substr(pos + 3)
            };
        }

        // ------------------------------------------------------------
        // Canonicalization (purely lexical)
        // ------------------------------------------------------------

        std::string canonicalize_path(const std::string& path)
        {
            fs::path p(path);
            return p.lexically_normal().generic_string();
        }

        // ------------------------------------------------------------
        // Path properties
        // ------------------------------------------------------------

        bool is_abspath(const std::string& path)
        {
            return fs::path(path).is_absolute();
        }

        bool is_root_path(const std::string& path)
        {
            fs::path p(path);
            return p.has_root_path() && p.relative_path().empty();
        }

        // ------------------------------------------------------------
        // Join
        // ------------------------------------------------------------

        std::string join(const std::string& base,
            const std::string& path)
        {
            fs::path p(path);

            if (p.is_absolute())
                return p.generic_string();

            return (fs::path(base) / p).generic_string();
        }

        // ------------------------------------------------------------
        // Directory / filename
        // ------------------------------------------------------------

        std::string basedir(const std::string& path)
        {
            fs::path p(path);

            if (p.has_parent_path())
                return p.parent_path().generic_string();

            return ".";
        }

        std::string basename(const std::string& path)
        {
            return fs::path(path).filename().generic_string();
        }

        std::string relpath(const std::string& base,
            const std::string& path)
        {
            return (fs::path(base).parent_path() / path).generic_string();
        }

        // ------------------------------------------------------------
        // Extension
        // ------------------------------------------------------------

        std::string ext(const std::string& path)
        {
            fs::path p(path);
            if (!p.has_extension())
                return {};

            return p.extension().string().substr(1); // remove dot
        }

        // ------------------------------------------------------------
        // Split
        // ------------------------------------------------------------

        std::pair<std::string, std::string>
            split(const std::string& path)
        {
            fs::path p(path);

            return {
                p.parent_path().empty() ? "." : p.parent_path().generic_string(),
                p.filename().generic_string()
            };
        }

        // ------------------------------------------------------------
        // Executable path (modernized)
        // ------------------------------------------------------------

        std::string get_executable_path()
        {
            return fs::current_path().generic_string();
        }

#ifdef _WIN32
        std::string to_utf8(const wchar_t* wstr, size_t len)
        {
            Util::SmallVector<char> char_buffer;
            auto ret = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
            if (ret < 0)
                return "";
            char_buffer.resize(ret);
            WideCharToMultiByte(CP_UTF8, 0, wstr, len, char_buffer.data(), char_buffer.size(), nullptr, nullptr);
            return std::string(char_buffer.data(), char_buffer.size());
        }

        std::wstring to_utf16(const char* str, size_t len)
        {
            Util::SmallVector<wchar_t> wchar_buffer;
            auto ret = MultiByteToWideChar(CP_UTF8, 0, str, len, nullptr, 0);
            if (ret < 0)
                return L"";
            wchar_buffer.resize(ret);
            MultiByteToWideChar(CP_UTF8, 0, str, len, wchar_buffer.data(), wchar_buffer.size());
            return std::wstring(wchar_buffer.data(), wchar_buffer.size());
        }

        std::string to_utf8(const std::wstring& wstr)
        {
            return to_utf8(wstr.data(), wstr.size());
        }

        std::wstring to_utf16(const std::string& str)
        {
            return to_utf16(str.data(), str.size());
        }
#endif
    }
}