#pragma once
#include "filesystem.h"
#include <unordered_map>
#include "util/small_vector.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


namespace FileSystem
{
	class MappedFile final : public File
	{
	public:
		static std::unique_ptr<File> open(const std::string& path, FileMode mode);
		~MappedFile() override;

		std::unique_ptr<FileMapping> map_subset(uint64_t offset, size_t range, std::shared_ptr<File> self) override;
		std::unique_ptr<FileMapping> map_write(size_t size, std::shared_ptr<File> self) override;
		void unmap(void* mapped, size_t range) override;
		uint64_t get_size() override;

	private:
		bool init(const std::string& path, FileMode mode);
		HANDLE file = INVALID_HANDLE_VALUE;
		HANDLE file_mapping = nullptr;
		uint64_t size = 0;
		std::string rename_from_on_close;
		std::string rename_to_on_close;
	};

	class OSFilesystem : public FilesystemBackend
	{
	public:
		OSFilesystem(const std::string& base);
		~OSFilesystem();
		Util::SmallVector<ListEntry> list(const std::string& path) override;
		std::unique_ptr<File> open(const std::string& path, FileMode mode) override;
		bool stat(const std::string& path, FileStat& stat) override;
		FileNotifyHandle install_notification(const std::string& path, std::function<void(const FileNotifyInfo&)> func) override;
		void uninstall_notification(FileNotifyHandle handle) override;
		void poll_notifications() override;
		int get_notification_fd() const override;
		std::string get_filesystem_path(const std::string& path) override;

		bool remove(const std::string& str) override;
		bool move_yield(const std::string& dst, const std::string& src) override;
		bool move_replace(const std::string& dst, const std::string& src) override;

	private:
		std::string base;

		struct Handler
		{
			std::string path;
			std::function<void(const FileNotifyInfo&)> func;
			HANDLE handle = nullptr;
			HANDLE event = nullptr;
			DWORD async_buffer[1024];
			OVERLAPPED overlapped;
		};

		std::unordered_map<FileNotifyHandle, Handler> handlers;
		FileNotifyHandle handle_id = 0;
		void kick_async(Handler& handler);
	};
}
