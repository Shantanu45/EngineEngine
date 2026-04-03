/*****************************************************************//**
 * \file   shader_container.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "shader_container.h"
#include "spirv_reflect.h"

namespace Vulkan
{
	const uint32_t RenderingShaderContainerVulkan::FORMAT_VERSION = 1;

	uint32_t RenderingShaderContainerVulkan::_format() const {
		return 0x43565053;
	}

	uint32_t RenderingShaderContainerVulkan::_format_version() const {
		return FORMAT_VERSION;
	}

	bool RenderingShaderContainerVulkan::_set_code_from_spirv(const ReflectShader& p_shader)
	{
		const std::vector<ReflectShaderStage>& p_spirv = p_shader.shader_stages;

		PackedByteArray code_bytes;
		shaders.resize(p_spirv.size());
		for (uint64_t i = 0; i < p_spirv.size(); i++) {
			RenderingShaderContainer::Shader& shader = shaders.data()[i];
			if (debug_info_enabled) {
				// Store SPIR-V as is when debug info is required.
				shader.code_compressed_bytes = p_spirv[i].spirv_data();
				shader.code_compression_flags = 0;
				shader.code_decompressed_size = 0;
			}
			else {
				// Encode into smolv.
				// TODO: 
				//std::span<uint8_t> spirv = reinterpret_cast<uint8_t*>(p_spirv[i].spirv().data(), p_spirv[i].spirv().size_bytes());//   reinterpret_cast<std::span<uint8_t>>(p_spirv[i].spirv())
				//smolv::ByteArray smolv_bytes;
				//bool smolv_encoded = smolv::Encode(spirv.data(), spirv.size(), smolv_bytes, smolv::kEncodeFlagStripDebugInfo);
				//ERR_FAIL_COND_V_MSG(!smolv_encoded, false, "Failed to compress SPIR-V into smolv.");

				//code_bytes.resize(smolv_bytes.size());
				//memcpy(code_bytes.data(), smolv_bytes.data(), code_bytes.size());

				//// Compress.
				//uint32_t compressed_size = 0;
				//shader.code_decompressed_size = code_bytes.size();
				//shader.code_compressed_bytes.resize(code_bytes.size());

				//bool compressed = compress_code(code_bytes.data(), code_bytes.size(), shader.code_compressed_bytes.data(), &compressed_size, &shader.code_compression_flags);
				//ERR_FAIL_COND_V_MSG(!compressed, false, std::format("Failed to compress native code to native for SPIR-V #%d.", i));

				//shader.code_compressed_bytes.resize(compressed_size);

				//// Indicate it uses smolv for compression.
				//shader.code_compression_flags |= COMPRESSION_FLAG_SMOLV;
			}

			shader.shader_stage = p_spirv[i].shader_stage;
		}
		return true;
	}

	RenderingShaderContainerVulkan::RenderingShaderContainerVulkan(bool p_debug_info_enabled) {
		debug_info_enabled = p_debug_info_enabled;
	}

	// RenderingShaderContainerFormatVulkan

	RenderingShaderContainer* RenderingShaderContainerFormatVulkan::create_container() const {
		return new RenderingShaderContainerVulkan(debug_info_enabled);
	}

	RenderingDeviceCommons::ShaderLanguageVersion RenderingShaderContainerFormatVulkan::get_shader_language_version() const {
		return RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1;
	}

	RenderingDeviceCommons::ShaderSpirvVersion RenderingShaderContainerFormatVulkan::get_shader_spirv_version() const {
		return RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_4;
	}

	void RenderingShaderContainerFormatVulkan::set_debug_info_enabled(bool p_debug_info_enabled) {
		debug_info_enabled = p_debug_info_enabled;
	}

	bool RenderingShaderContainerFormatVulkan::get_debug_info_enabled() const {
		return debug_info_enabled;
	}

	RenderingShaderContainerFormatVulkan::RenderingShaderContainerFormatVulkan() {}

	RenderingShaderContainerFormatVulkan::~RenderingShaderContainerFormatVulkan() {}
}
