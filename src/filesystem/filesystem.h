/*****************************************************************//**
 * \file   filesystem.h
 * \brief  This is a virtual filesystem abstraction layer in C++. 
 * It lets you work with files from multiple sources (disk, memory, blobs, etc.) through a single unified API.
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/


#pragma once
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <stdio.h>

#include "util/logger.h"
#include "util/small_vector.h"

namespace FileSystem
{
	class FileMapping;

	// is an abstract base class representing a file that can be memory-mapped
	class File
	{
	public:
		virtual ~File() = default;
		virtual std::unique_ptr<FileMapping> map_subset(uint64_t offset, size_t range,
			std::shared_ptr<File> self) = 0;
		virtual std::unique_ptr<FileMapping> map_write(size_t size,
			std::shared_ptr<File> self) = 0;
		virtual uint64_t get_size() = 0;

		// Only called by FileMapping.
		virtual void unmap(void* mapped, size_t range) = 0;

		std::unique_ptr<FileMapping> map(std::shared_ptr<File> self);
	};


	class FileMapping
	{
	public:
		template <typename T = void>
		inline const T* data() const
		{
			void* ptr = static_cast<uint8_t*>(mapped) + map_offset;
			return static_cast<const T*>(ptr);
		}

		template <typename T = void>
		inline T* mutable_data()
		{
			void* ptr = static_cast<uint8_t*>(mapped) + map_offset;
			return static_cast<T*>(ptr);
		}

		uint64_t get_file_offset() const;
		uint64_t get_size() const;

		~FileMapping();
		FileMapping(std::shared_ptr<File> handle,
			uint64_t file_offset,
			void* mapped, size_t mapped_size,
			size_t map_offset, size_t accessible_size);

	private:
		std::shared_ptr<File> handle; // non-owning; File outlives its mappings
		uint64_t file_offset;
		void* mapped;
		size_t mapped_size;
		size_t map_offset;
		size_t accessible_size;
	};


	enum class PathType { File, Directory, Special };

	struct ListEntry { std::string path; PathType type; };
	struct FileStat { uint64_t size; PathType type; uint64_t last_modified; };

	using FileNotifyHandle = int;
	enum class FileNotifyType { FileChanged, FileDeleted, FileCreated };
	struct FileNotifyInfo { std::string path; FileNotifyType type; FileNotifyHandle handle; };

	enum class FileMode { ReadOnly, WriteOnly, ReadWrite, WriteOnlyTransactional };


	class FilesystemBackend
	{
	public:
		virtual ~FilesystemBackend() = default;
		Util::SmallVector<ListEntry> walk(const std::string& path);
		virtual Util::SmallVector<ListEntry> list(const std::string& path) = 0;
		virtual std::unique_ptr<File> open(const std::string& path, FileMode mode = FileMode::ReadOnly) = 0;
		virtual bool stat(const std::string& path, FileStat& stat) = 0;

		virtual FileNotifyHandle
			install_notification(const std::string& path, std::function<void(const FileNotifyInfo&)> func) = 0;

		virtual void uninstall_notification(FileNotifyHandle handle) = 0;
		virtual void poll_notifications() = 0;
		virtual int get_notification_fd() const = 0;

		virtual bool remove(const std::string& path);
		virtual bool move_replace(const std::string& dst, const std::string& src);
		virtual bool move_yield(const std::string& dst, const std::string& src);

		inline virtual std::string get_filesystem_path(const std::string&) { return ""; }

		void set_protocol(const std::string& proto) { protocol = proto; }

	protected:
		std::string protocol;
	};


	class FilesystemInterface
	{
	public:
		virtual ~FilesystemInterface() = default;
		virtual bool load_text_file(const std::string& path, std::string& str) = 0;
		virtual std::string get_filesystem_path(const std::string& path) { return path; }
	};

	// main dispatcher that routes operations to the appropriate backend based on protocol (e.g., "file://", "blob://")
	// Supports multiple protocols registered at runtime
	class Filesystem final : public FilesystemInterface
	{
	public:
		/**
		 * Initializes the dispatcher by registering default backends.
		 * file://
		 * memory://
		 * builtin://
		 * cache://
		 *
		 */
		Filesystem();

		/**
		 * Backends are registered at runtime via this fuction.
		 *
		 * \param proto
		 * \param fs
		 */
		void register_protocol(const std::string& proto, std::unique_ptr<FilesystemBackend> fs);
		FilesystemBackend* get_backend(const std::string& proto);

		/**
		 * A more sophisticated initializer meant to be called after construction. It:

			Finds the executable's directory
			Looks for assets/ and builtin/assets/ subdirectories next to the executable
			Registers them as assets:// and builtin:// if found
			Registers a cache:// directory alongside assets://
			Throws if builtin:// or cache:// are missing at the end - these are considered mandatory.
		 *
		 * \param fs
		 * \param default_asset_directory
		 */
		static void setup_default_filesystem(Filesystem* fs, const char* default_asset_directory);

		/**
		 * Same as list() but recursive - descends into subdirectories.
		 * The actual recursion logic lives in FilesystemBackend::walk()..
		 *
		 * \param path
		 * \return
		 */
		Util::SmallVector<ListEntry> walk(const std::string& path);
		/**
		 * Returns the immediate children of a directory (non-recursive).
		 *  Splits the protocol from the path, finds the backend, delegates to backend->list().
		 *
		 * \param path
		 * \return
		 */
		Util::SmallVector<ListEntry> list(const std::string& path);

		/**
		 * The core open primitive. Splits the protocol, finds the backend, returns a raw File*.
		 * The caller is responsible for the lifetime. Most code uses the mapping helpers below instead..
		 *
		 * \param path
		 * \param mode
		 * \return
		 */
		std::unique_ptr<File> open(const std::string& path, FileMode mode = FileMode::ReadOnly);

		/**
		 * These are the preferred way to access file contents-they open the file and immediately memory-map it..
		 * Opens in ReadOnly mode and calls file->map() (maps the whole file).
		 * \param path
		 * \return
		 */
		std::unique_ptr<FileMapping> open_readonly_mapping(const std::string& path);
		/**
		 * Opens in WriteOnly mode and calls file->map_write(size)-allocates size bytes for writing..
		 *
		 * \param path
		 * \param size
		 * \return
		 */
		std::unique_ptr<FileMapping> open_writeonly_mapping(const std::string& path, size_t size);
		/**
		 * Same as write-only but uses WriteOnlyTransactional mode-on backends that support it,
		 * the write is atomic (e.g. write to a temp file, then rename on unmap).
		 *
		 * \param path
		 * \param size
		 * \return
		 */
		std::unique_ptr<FileMapping> open_transactional_mapping(const std::string& path, size_t size);


		const std::unordered_map<std::string, std::unique_ptr<FilesystemBackend>>& get_protocols() const
		{
			return protocols;
		}

		/**
		 * Opens a readonly mapping, copies all bytes into str,
		 * then strips \r characters to normalize Windows line endings.
		 * Returns false if the file couldn't be opened..
		 *
		 * \param path
		 * \param str
		 * \return
		 */
		bool read_file_to_string(const std::string& path, std::string& str);
		/**
		 * Thin wrapper-just calls write_buffer_to_file with the string's data and size..
		 *
		 * \param path
		 * \param str
		 * \return
		 */
		bool write_string_to_file(const std::string& path, const std::string& str);
		/**
		 * Opens a transactional mapping of size bytes and memcpys data into it. Returns false on failure..
		 *
		 * \param path
		 * \param data
		 * \param size
		 * \return
		 */
		bool write_buffer_to_file(const std::string& path, const void* data, size_t size);

		/**
		 * Fills a FileStat struct with file metadata: size, type (File/Directory/Special),
		 * and last_modified. Returns false if the path doesn't exist..
		 *
		 * \param path
		 * \param stat
		 * \return
		 */
		bool stat(const std::string& path, FileStat& stat);

		/**
		 * Returns the real underlying OS path for a virtual path, if the backend supports it. Most backends return "" -
		 * only OSFilesystem gives a meaningful result.
		 * Useful for passing paths to third-party libraries that need a real path.
		 *
		 * \param path
		 * \return
		 */
		std::string get_filesystem_path(const std::string& path) override;

		/**
		 * Iterates over all registered backends and calls poll_notifications() on each.
		 * This is how file-change events are pumped-you'd call this once per frame in a game loop, for example..
		 *
		 */
		void poll_notifications();
	private:

		/**
		 * Read-only accessor to the full protocol map.
		 * Useful for iterating all registered backends, e.g. for debugging or serialization.
		 */
		std::unordered_map<std::string, std::unique_ptr<FilesystemBackend>> protocols;

		/**
		 * Implements the FilesystemInterface virtual method-just forwards to read_file_to_string.
		 * This is what allows Filesystem to be used polymorphically wherever a FilesystemInterface* is expected..
		 *
		 * \param path
		 * \param str
		 * \return
		 */
		bool load_text_file(const std::string& path, std::string& str) override;

		/**
		 * Deletes a file. Delegates to the backend-note that FilesystemBackend::remove() returns false by default,
		 * so only backends that override it (like OSFilesystem) actually support this..
		 *
		 * \param path
		 * \return
		 */
		bool remove(const std::string& path);
		/**
		 * Moves src to dst, overwriting the destination if it exists.
		 *  Requires both paths to resolve to the same backend-cross-protocol moves return false..
		 *
		 * \param dst
		 * \param src
		 * \return
		 */
		bool move_replace(const std::string& dst, const std::string& src);
		/**
		 * Same as move_replace but does not overwrite -
		 * yields if the destination already exists. Same same-backend restriction applies..
		 *
		 * \param dst
		 * \param src
		 * \return
		 */
		bool move_yield(const std::string& dst, const std::string& src);
	};

	 //memory://		In-memory vector<uint8_t> map.
	class ScratchFilesystem final : public FilesystemBackend
	{
	public:
		Util::SmallVector<ListEntry> list(const std::string& path) override;
		std::unique_ptr<File> open(const std::string& path, FileMode mode = FileMode::ReadOnly) override;
		bool stat(const std::string& path, FileStat& stat) override;
		FileNotifyHandle install_notification(const std::string& path, std::function<void(const FileNotifyInfo&)> func) override;
		void uninstall_notification(FileNotifyHandle handle) override;
		void poll_notifications() override;
		int get_notification_fd() const override;

	private:
		struct ScratchFile { Util::SmallVector<uint8_t> data; };
		std::unordered_map<std::string, std::unique_ptr<ScratchFile>> scratch_files;
	};

	// Parses a packed binary blob format containing a directory structure
	// Stores file metadata (path, offset, size) and reconstructs a filesystem tree
	class BlobFilesystem final : public FilesystemBackend
	{
	public:
		explicit BlobFilesystem(std::unique_ptr<File> file);

		Util::SmallVector<ListEntry> list(const std::string& path) override;
		std::unique_ptr<File> open(const std::string& path, FileMode mode) override;
		bool stat(const std::string& path, FileStat& stat) override;
		FileNotifyHandle install_notification(const std::string& path, std::function<void(const FileNotifyInfo&)> func) override;
		void uninstall_notification(FileNotifyHandle handle) override;
		void poll_notifications() override;
		int get_notification_fd() const override;

	private:
		std::shared_ptr<File> file; // owns the underlying blob file
		size_t blob_base_offset = 0;

		struct BlobFile { std::string path; size_t offset; size_t size; };

		struct Directory
		{
			std::string path;
			Util::SmallVector<std::unique_ptr<Directory>> dirs;
			Util::SmallVector<BlobFile> files;
		};
		std::unique_ptr<Directory> root;
		BlobFile* find_file(const std::string& path);
		Directory* find_directory(const std::string& path);
		Directory* make_directory(const std::string& path);
		void parse();

		static uint8_t     read_u8(const uint8_t*& buf, size_t& size);
		static uint64_t    read_u64(const uint8_t*& buf, size_t& size);
		static std::string read_string(const uint8_t*& buf, size_t& size, size_t len);
		void add_entry(const std::string& path, size_t offset, size_t size);
	};


	// Wraps a vector<uint8_t> as a File.
	class ScratchFilesystemFile final : public File
	{
	public:
		explicit ScratchFilesystemFile(Util::SmallVector<uint8_t>& data_) : data(data_) {}

		std::unique_ptr<FileMapping> map_subset(uint64_t offset, size_t range,
			std::shared_ptr<File> self) override
		{
			if (offset + range > data.size()) return nullptr;
			return std::make_unique<FileMapping>(
				nullptr,   // FileMapping now co-owns the File
				offset,
				data.data() + offset, range,
				0, range);
		}

		std::unique_ptr<FileMapping> map_write(size_t size, std::shared_ptr<File> self) override
		{
			data.resize(size);
			return map_subset(0, size, self);
		}

		void unmap(void*, size_t) override {}

		uint64_t get_size() override { return data.size(); }

		Util::SmallVector<uint8_t>& data;
	};


	// Read-only view over a pre-existing memory region.
	class ConstantMemoryFile final : public File
	{
	public:
		ConstantMemoryFile(const void* mapped_, size_t size_)
			: mapped(static_cast<const uint8_t*>(mapped_)), size(size_) {
		}

		std::unique_ptr<FileMapping> map_subset(uint64_t offset, size_t range,
			std::shared_ptr<File> self) override
		{
			if (offset + range > size)
				return nullptr;
			return std::make_unique<FileMapping>(
				nullptr, offset,
				const_cast<uint8_t*>(mapped) + offset, range,
				0, range);
		}

		std::unique_ptr<FileMapping> map_write(size_t, std::shared_ptr<File> self) override { return nullptr; }
		void unmap(void*, size_t) override {}
		uint64_t get_size() override { return size; }

	private:
		const uint8_t* mapped;
		size_t size;
	};


	// Presents a byte range of a larger File as an independent File.
	class FileSlice final : public File
	{
	public:
		// Takes shared (non-owning) access to the parent file.
		// The parent must outlive all FileSlices derived from it.
		FileSlice(File* handle, uint64_t offset, uint64_t range);

		std::unique_ptr<FileMapping> map_subset(uint64_t offset, size_t range, std::shared_ptr<File> self) override;
		std::unique_ptr<FileMapping> map_write(size_t, std::shared_ptr<File> self) override;
		void unmap(void*, size_t) override;
		uint64_t get_size() override;

	private:
		File* handle; // non-owning; BlobFilesystem owns the root File
		uint64_t offset;
		uint64_t range;
	};
}