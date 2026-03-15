#include "shader_container.h"
#include ""

namespace Vulkan
{
	const uint32_t RenderingShaderContainer::FORMAT_VERSION = 1;

	RenderingShaderContainer* RenderingShaderContainerFormatVulkan::create_container() const
	{
		return new RenderingShaderContainer(debug_info_enabled);
	}
	ShaderLanguageVersion RenderingShaderContainerFormatVulkan::get_shader_language_version() const
	{
		return SHADER_LANGUAGE_VULKAN_VERSION_1_1;

	}
	ShaderSpirvVersion RenderingShaderContainerFormatVulkan::get_shader_spirv_version() const
	{
		return SHADER_SPIRV_VERSION_1_4;

	}
	void RenderingShaderContainerFormatVulkan::set_debug_info_enabled(bool p_debug_info_enabled)
	{
		debug_info_enabled = p_debug_info_enabled;
	}
	bool RenderingShaderContainerFormatVulkan::get_debug_info_enabled() const
	{
		return debug_info_enabled;
	}
	RenderingShaderContainerFormatVulkan::RenderingShaderContainerFormatVulkan() {
	}
	
	RenderingShaderContainerFormatVulkan::~RenderingShaderContainerFormatVulkan() {}

	RenderingShaderContainer::RenderingShaderContainer(bool p_debug_info_enabled) {
		debug_info_enabled = p_debug_info_enabled;
	}

	uint32_t RenderingShaderContainer::_format() const
	{
		return 0x43565053;
	}
	uint32_t RenderingShaderContainer::_format_version() const
	{
		return FORMAT_VERSION;
	}
	RenderingShaderContainer::RenderingShaderContainer()
	{
	}

	void RenderingShaderContainer::_set_from_shader_reflection_post(const ReflectShader& p_shader)
	{
		// Do nothing!
	}

	bool RenderingShaderContainer::_set_code_from_spirv(const ReflectShader& p_shader)
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
				//bool smolv_encoded = smolv::Encode(spirv.ptr(), spirv.size(), smolv_bytes, smolv::kEncodeFlagStripDebugInfo);
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
	}

	void RenderingShaderContainer::set_from_shader_reflection(const ReflectShader& p_reflection)
	{
		reflection_binding_set_uniforms_count.clear();
		reflection_binding_set_uniforms_data.clear();
		reflection_specialization_data.clear();
		reflection_shader_stages.clear();

		reflection_data.vertex_input_mask = p_reflection.vertex_input_mask;
		reflection_data.fragment_output_mask = p_reflection.fragment_output_mask;
		reflection_data.specialization_constants_count = p_reflection.specialization_constants.size();
		reflection_data.pipeline_type = p_reflection.pipeline_type;
		reflection_data.has_multiview = p_reflection.has_multiview;
		reflection_data.has_dynamic_buffers = p_reflection.has_dynamic_buffers;
		reflection_data.compute_local_size[0] = p_reflection.compute_local_size[0];
		reflection_data.compute_local_size[1] = p_reflection.compute_local_size[1];
		reflection_data.compute_local_size[2] = p_reflection.compute_local_size[2];
		reflection_data.set_count = p_reflection.uniform_sets.size();
		reflection_data.push_constant_size = p_reflection.push_constant_size;
		reflection_data.push_constant_stages_mask = uint32_t(p_reflection.push_constant_stages);
		reflection_data.shader_name_len = shader_name.length();

		ReflectionBindingData binding_data;
		for (const ReflectDescriptorSet& uniform_set : p_reflection.uniform_sets) {
			for (const ReflectUniform& uniform : uniform_set) {
				binding_data.type = uint32_t(uniform.type);
				binding_data.binding = uniform.binding;
				binding_data.stages = uint32_t(uniform.stages);
				binding_data.length = uniform.length;
				binding_data.writable = uint32_t(uniform.writable);
				reflection_binding_set_uniforms_data.push_back(binding_data);
			}

			reflection_binding_set_uniforms_count.push_back(uniform_set.size());
		}

		ReflectionSpecializationData specialization_data;
		for (const ReflectSpecializationConstant& spec : p_reflection.specialization_constants) {
			specialization_data.type = uint32_t(spec.type);
			specialization_data.constant_id = spec.constant_id;
			specialization_data.int_value = spec.int_value;
			specialization_data.stage_flags = uint32_t(spec.stages);
			reflection_specialization_data.push_back(specialization_data);
		}

		for (uint32_t i = 0; i < RDC::SHADER_STAGE_MAX; i++) {
			if (p_reflection.stages_bits.has_flag(RDC::ShaderStage(1U << i))) {
				reflection_shader_stages.push_back(RDC::ShaderStage(i));
			}
		}

		reflection_data.stage_count = reflection_shader_stages.size();

		_set_from_shader_reflection_post(p_reflection);
	}

	Error RenderingShaderContainer::reflect_spirv(const String& p_shader_name, std::span<Device::ShaderStageSPIRVData> p_spirv, ReflectShader& r_shader)
	{
		return Error();
	}
}
