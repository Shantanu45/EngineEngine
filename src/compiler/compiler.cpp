/*****************************************************************//**
 * \file   compiler.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "compiler.h"
#include "util/small_vector.h"
#include "shaderc/shaderc.hpp"

namespace Compiler
{
	GLSLCompiler::GLSLCompiler(FilesystemInterface& iface_)
		: iface(iface_)
	{
	}

	static Stage convert_stage(const std::string& stage)
	{
		if (stage == "vertex")
			return Stage::Vertex;
		else if (stage == "tess_control")
			return Stage::TessControl;
		else if (stage == "tess_evaluation")
			return Stage::TessEvaluation;
		else if (stage == "geometry")
			return Stage::Geometry;
		else if (stage == "compute")
			return Stage::Compute;
		else if (stage == "fragment")
			return Stage::Fragment;
		else if (stage == "task")
			return Stage::Task;
		else if (stage == "mesh")
			return Stage::Mesh;
		else
			return Stage::Unknown;
	}

	bool GLSLCompiler::set_source_from_file(const std::string& path, Stage forced_stage)
	{
		if (!iface.load_text_file(path, source))
		{
			LOGE("Failed to load shader: %s\n", path.c_str());
			return false;
		}

		source_path = path;

		if (forced_stage != Stage::Unknown)
			stage = forced_stage;
		else
			stage = ::Compiler::stage_from_path(path);

		return stage != Stage::Unknown;
	}

	bool GLSLCompiler::set_source_from_file_multistage(const std::string& path)
	{
		if (!iface.load_text_file(path, source))
		{
			LOGE("Failed to load shader: %s\n", path.c_str());
			return false;
		}

		source_path = path;
		stage = Stage::Unknown;
		return true;
	}

	void GLSLCompiler::set_include_directories(const Util::SmallVector<std::string>* include_directories_)
	{
		include_directories = include_directories_;
	}

	bool GLSLCompiler::find_include_path(const std::string& source_path_, const std::string& include_path,
		std::string& included_path, std::string& included_source)
	{
		auto relpath = Path::relpath(source_path_, include_path);
		if (iface.load_text_file(relpath, included_source))
		{
			included_path = relpath;
			return true;
		}

		if (include_directories)
		{
			for (auto& include_dir : *include_directories)
			{
				auto path = Path::join(include_dir, include_path);
				if (iface.load_text_file(path, included_source))
				{
					included_path = path;
					return true;
				}
			}
		}

		return false;
	}

	bool GLSLCompiler::parse_variants(const std::string& source_, const std::string& path)
	{
		auto lines = Path::split(source_, "\n");

		unsigned line_index = 1;
		size_t offset;

		for (auto& line : lines)
		{
			// This check, followed by the include statement check below isn't even remotely correct,
			// but we only have to care about shaders we control here.
			if ((offset = line.find("//")) != std::string::npos)
				line = line.substr(0, offset);

			if ((offset = line.find("#include \"")) != std::string::npos)
			{
				/*auto include_path = line.substr(offset + 10);*/
				size_t start = offset + 10;
				size_t end = line.find('"', start);
				if (end == std::string::npos) { 
					/* error */ 
				}
				auto include_path = line.substr(start, end - start);
				if (!include_path.empty() && include_path.back() == '"')
					include_path.pop_back();

				std::string included_source;
				if (!find_include_path(path, include_path, include_path, included_source))
				{
					LOGE("Failed to include GLSL file: %s\n", include_path.c_str());
					return false;
				}

				preprocessed_source += "#line " + std::to_string(line_index + 1) + " \"" + include_path + "\"\n";
				if (!parse_variants(included_source, include_path))
					return false;
				preprocessed_source += "#line " + std::to_string(line_index + 1) + " \"" + path + "\"\n";

				dependencies.insert(include_path);
			}
			else if (line.find("#pragma optimize off") == 0)
			{
				optimization = Optimization::ForceOff;
				preprocessed_source += "// #pragma optimize off";
				preprocessed_source += '\n';
			}
			else if (line.find("#pragma optimize on") == 0)
			{
				optimization = Optimization::ForceOn;
				preprocessed_source += "// #pragma optimize on";
				preprocessed_source += '\n';
			}
			else if (line.find("#pragma stage ") == 0)
			{
				if (!preprocessed_source.empty())
				{
					preprocessed_sections.push_back({ preprocessing_active_stage, std::move(preprocessed_source) });
					preprocessed_source = {};
				}
				auto stage_name = line.substr(14);
				stage_name.erase(stage_name.find_last_not_of(" \t\r\n") + 1);
				preprocessing_active_stage = convert_stage(stage_name);
				//preprocessing_active_stage = convert_stage(line.substr(14));
				preprocessed_source += "#line " + std::to_string(line_index + 1) + " \"" + path + "\"\n";
			}
			else if (line.find("#pragma ") == 0)
			{
				pragmas.push_back(line.substr(8));
				preprocessed_source += "// ";
				preprocessed_source += line;
				preprocessed_source += '\n';
			}
			else
			{
				preprocessed_source += line;
				preprocessed_source += '\n';

				auto first_non_space = line.find_first_not_of(' ');
				if (first_non_space != std::string::npos && line[first_non_space] == '#')
				{
					auto keywords = Path::split(line.substr(first_non_space + 1), " ");
					if (keywords.size() == 1)
					{
						auto& word = keywords.front();
						if (word == "endif")
							preprocessed_source += "#line " + std::to_string(line_index + 1) + " \"" + path + "\"\n";
					}
				}
			}

			line_index++;
		}
		return true;
	}

	bool GLSLCompiler::preprocess()
	{
		// Use a custom preprocessor where we only resolve includes once.
		// The builtin glslang preprocessor is not suitable for this task,
		// since we need to defer resolving defines.

		preprocessed_source.clear();
		pragmas.clear();
		preprocessed_sections.clear();
		preprocessing_active_stage = Stage::Unknown;

		bool ret = parse_variants(source, source_path);

		if (ret && !preprocessed_source.empty())
		{
			preprocessed_sections.push_back({ preprocessing_active_stage, std::move(preprocessed_source) });
			preprocessed_source = {};
		}

		return ret;
	}

	uint64_t GLSLCompiler::get_source_hash() const
	{
		XXH64_state_t* state = XXH64_createState();
		XXH64_reset(state, 0); // 0 = seed

		for (auto& section : preprocessed_sections)
		{
			uint32_t stage = uint32_t(section.stage);
			XXH64_update(state, &stage, sizeof(stage));
			XXH64_update(state, section.source.data(), section.source.size());
		}

		XXH64_update(state, preprocessed_source.data(), preprocessed_source.size());

		uint64_t hash = XXH64_digest(state);
		XXH64_freeState(state);
		return hash;
	}

	Util::SmallVector<uint32_t> GLSLCompiler::compile(std::string& error_message, const Util::SmallVector<std::pair<std::string, int>>* defines) const
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		if (preprocessed_sections.empty())
		{
			error_message = "Need to preprocess source first.";
			return {};
		}

		if (defines)
			for (auto& define : *defines)
				options.AddMacroDefinition(define.first, std::to_string(define.second));

#if MAKER_COMPILER_OPTIMIZE
		if (optimization != Optimization::ForceOff)
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
		else
			options.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
		options.SetOptimizationLevel(shaderc_optimization_level_zero);
#endif

		if (!strip && optimization == Optimization::ForceOff)
			options.SetGenerateDebugInfo();

		shaderc_shader_kind kind;
		switch (stage)
		{
		case Stage::Vertex:
			kind = shaderc_glsl_vertex_shader;
			break;

		case Stage::TessControl:
			kind = shaderc_glsl_tess_control_shader;
			break;

		case Stage::TessEvaluation:
			kind = shaderc_glsl_tess_evaluation_shader;
			break;

		case Stage::Geometry:
			kind = shaderc_glsl_geometry_shader;
			break;

		case Stage::Fragment:
			kind = shaderc_glsl_fragment_shader;
			break;

		case Stage::Compute:
			kind = shaderc_glsl_compute_shader;
			break;

		case Stage::Task:
			kind = shaderc_glsl_task_shader;
			break;

		case Stage::Mesh:
			kind = shaderc_glsl_mesh_shader;
			break;

		default:
			error_message = "Invalid shader stage.";
			return {};
		}

		auto env = shaderc_env_version_vulkan_1_1;
		auto spv_version = shaderc_spirv_version_1_3;

		if (target == Target::Vulkan12)
		{
			env = shaderc_env_version_vulkan_1_2;
			spv_version = shaderc_spirv_version_1_5;
		}
		else if (target == Target::Vulkan13)
		{
			env = shaderc_env_version_vulkan_1_3;
			spv_version = shaderc_spirv_version_1_6;
		}

		options.SetTargetEnvironment(shaderc_target_env_vulkan, env);
		options.SetTargetSpirv(spv_version);
		options.SetSourceLanguage(shaderc_source_language_glsl);

		shaderc::SpvCompilationResult result;

		if (preprocessed_sections.size() == 1)
		{
			if (preprocessed_sections.front().stage != Stage::Unknown && preprocessed_sections.front().stage != stage)
			{
				error_message = "No preprocessed sections available.";
				return {};
			}
			result = compiler.CompileGlslToSpv(preprocessed_sections.front().source, kind, source_path.c_str(), options);
		}
		else
		{
			std::string combined_source;
			for (auto& section : preprocessed_sections)
				if (section.stage == Stage::Unknown || section.stage == stage)
					combined_source += section.source;
			if (combined_source.empty())
			{
				error_message = "No preprocessed sections available.";
				return {};
			}
			result = compiler.CompileGlslToSpv(combined_source, kind, source_path.c_str(), options);
		}

		error_message.clear();
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			error_message = result.GetErrorMessage();
			return {};
		}

		Util::SmallVector<uint32_t> compiled_spirv(result.cbegin(), result.cend());

#if 0
		spvtools::SpirvTools core(target == Target::Vulkan13 ? SPV_ENV_VULKAN_1_3 : SPV_ENV_VULKAN_1_1);
		core.SetMessageConsumer([&error_message](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
			error_message = message;
			});

		spvtools::ValidatorOptions opts;
		opts.SetScalarBlockLayout(true);
		if (!core.Validate(compiled_spirv.data(), compiled_spirv.size(), opts))
		{
			error_message += "\nFailed to validate SPIR-V.\n";
			return {};
		}
#endif

		return compiled_spirv;
	}
}
