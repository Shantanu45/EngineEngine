/*****************************************************************//**
 * \file   compiler.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <stdint.h>
#include "filesystem/filesystem.h"
#include "xxhash.h"
#include "filesystem/path_utils.h"

using namespace FileSystem;
namespace Compiler
{

	enum class Stage
	{
		Vertex,
		TessControl,
		TessEvaluation,
		Geometry,
		Fragment,
		Compute,
		Task,
		Mesh,
		Unknown
	};

	inline static Stage stage_from_path(const std::string& path)
	{
		auto ext = Path::ext(path);
		if (ext == "vert")
			return Stage::Vertex;
		else if (ext == "tesc")
			return Stage::TessControl;
		else if (ext == "tese")
			return Stage::TessEvaluation;
		else if (ext == "geom")
			return Stage::Geometry;
		else if (ext == "frag")
			return Stage::Fragment;
		else if (ext == "comp")
			return Stage::Compute;
		else if (ext == "task")
			return Stage::Task;
		else if (ext == "mesh")
			return Stage::Mesh;
		else
			return Stage::Unknown;
	}

	enum class Target
	{
		Vulkan11,
		Vulkan12,
		Vulkan13
	};

	class GLSLCompiler
	{
	public:

		static Stage stage_from_path(const std::string& path);
		explicit GLSLCompiler(FilesystemInterface& iface);

		void set_target(Target target_)
		{
			target = target_;
		}

		void set_stage(Stage stage_)
		{
			stage = stage_;
		}

		void set_source(std::string source_, std::string path)
		{
			source = std::move(source_);
			source_path = std::move(path);
		}

		void set_include_directories(const std::vector<std::string>* include_directories);

		bool set_source_from_file(const std::string& path, Stage stage = Stage::Unknown);
		bool set_source_from_file_multistage(const std::string& path);
		bool preprocess();
		uint64_t get_source_hash() const;

		std::vector<uint32_t> compile(std::string& error_message, const std::vector<std::pair<std::string, int>>* defines = nullptr) const;

		const std::unordered_set<std::string>& get_dependencies() const
		{
			return dependencies;
		}

		enum class Optimization
		{
			ForceOff,
			ForceOn,
			Default
		};

		void set_optimization(Optimization opt)
		{
			optimization = opt;
		}

		void set_strip(bool strip_)
		{
			strip = strip_;
		}

		const std::vector<std::string>& get_user_pragmas() const
		{
			return pragmas;
		}

	private:
		FilesystemInterface& iface;
		std::string source;
		std::string source_path;
		const std::vector<std::string>* include_directories = nullptr;
		Stage stage = Stage::Unknown;

		std::unordered_set<std::string> dependencies;
		struct Section
		{
			Stage stage;
			std::string source;
		};
		std::vector<Section> preprocessed_sections;
		std::string preprocessed_source;
		Stage preprocessing_active_stage = Stage::Unknown;

		std::vector<std::pair<size_t, size_t>> preprocessed_lines;
		std::vector<std::string> pragmas;

		Target target = Target::Vulkan11;

		bool parse_variants(const std::string& source, const std::string& path);

		Optimization optimization = Optimization::Default;
		bool strip = false;

		bool find_include_path(const std::string& source_path, const std::string& include_path,
			std::string& included_path, std::string& included_source);
	};
}
