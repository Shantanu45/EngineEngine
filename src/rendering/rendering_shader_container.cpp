#include "rendering_shader_container.h"
#include "spirv_reflect.h"

namespace Rendering
{
	static RenderingDeviceCommons::DataFormat spv_image_format_to_data_format(const SpvImageFormat p_format) {
		using RDC = RenderingDeviceCommons;
		switch (p_format) {
		case SpvImageFormatUnknown:
			return RDC::DATA_FORMAT_MAX;
		case SpvImageFormatRgba32f:
			return RDC::DATA_FORMAT_R32G32B32A32_SFLOAT;
		case SpvImageFormatRgba16f:
			return RDC::DATA_FORMAT_R16G16B16A16_SFLOAT;
		case SpvImageFormatR32f:
			return RDC::DATA_FORMAT_R32_SFLOAT;
		case SpvImageFormatRgba8:
			return RDC::DATA_FORMAT_R8G8B8A8_UNORM;
		case SpvImageFormatRgba8Snorm:
			return RDC::DATA_FORMAT_R8G8B8A8_SNORM;
		case SpvImageFormatRg32f:
			return RDC::DATA_FORMAT_R32G32_SFLOAT;
		case SpvImageFormatRg16f:
			return RDC::DATA_FORMAT_R16G16_SFLOAT;
		case SpvImageFormatR11fG11fB10f:
			return RDC::DATA_FORMAT_B10G11R11_UFLOAT_PACK32;
		case SpvImageFormatR16f:
			return RDC::DATA_FORMAT_R16_SFLOAT;
		case SpvImageFormatRgba16:
			return RDC::DATA_FORMAT_R16G16B16A16_UNORM;
		case SpvImageFormatRgb10A2:
			return RDC::DATA_FORMAT_A2B10G10R10_UNORM_PACK32;
		case SpvImageFormatRg16:
			return RDC::DATA_FORMAT_R16G16_UNORM;
		case SpvImageFormatRg8:
			return RDC::DATA_FORMAT_R8G8_UNORM;
		case SpvImageFormatR16:
			return RDC::DATA_FORMAT_R16_UNORM;
		case SpvImageFormatR8:
			return RDC::DATA_FORMAT_R8_UNORM;
		case SpvImageFormatRgba16Snorm:
			return RDC::DATA_FORMAT_R16G16B16A16_SNORM;
		case SpvImageFormatRg16Snorm:
			return RDC::DATA_FORMAT_R16G16_SNORM;
		case SpvImageFormatRg8Snorm:
			return RDC::DATA_FORMAT_R8G8_SNORM;
		case SpvImageFormatR16Snorm:
			return RDC::DATA_FORMAT_R16_SNORM;
		case SpvImageFormatR8Snorm:
			return RDC::DATA_FORMAT_R8_SNORM;
		case SpvImageFormatRgba32i:
			return RDC::DATA_FORMAT_R32G32B32A32_SINT;
		case SpvImageFormatRgba16i:
			return RDC::DATA_FORMAT_R16G16B16A16_SINT;
		case SpvImageFormatRgba8i:
			return RDC::DATA_FORMAT_R8G8B8A8_SINT;
		case SpvImageFormatR32i:
			return RDC::DATA_FORMAT_R32_SINT;
		case SpvImageFormatRg32i:
			return RDC::DATA_FORMAT_R32G32_SINT;
		case SpvImageFormatRg16i:
			return RDC::DATA_FORMAT_R16G16_SINT;
		case SpvImageFormatRg8i:
			return RDC::DATA_FORMAT_R8G8_SINT;
		case SpvImageFormatR16i:
			return RDC::DATA_FORMAT_R16_SINT;
		case SpvImageFormatR8i:
			return RDC::DATA_FORMAT_R8_SINT;
		case SpvImageFormatRgba32ui:
			return RDC::DATA_FORMAT_R32G32B32A32_UINT;
		case SpvImageFormatRgba16ui:
			return RDC::DATA_FORMAT_R16G16B16A16_UINT;
		case SpvImageFormatRgba8ui:
			return RDC::DATA_FORMAT_R8G8B8A8_UINT;
		case SpvImageFormatR32ui:
			return RDC::DATA_FORMAT_R32_UINT;
		case SpvImageFormatRgb10a2ui:
			return RDC::DATA_FORMAT_A2B10G10R10_UINT_PACK32;
		case SpvImageFormatRg32ui:
			return RDC::DATA_FORMAT_R32G32_UINT;
		case SpvImageFormatRg16ui:
			return RDC::DATA_FORMAT_R16G16_UINT;
		case SpvImageFormatRg8ui:
			return RDC::DATA_FORMAT_R8G8_UINT;
		case SpvImageFormatR16ui:
			return RDC::DATA_FORMAT_R16_UINT;
		case SpvImageFormatR8ui:
			return RDC::DATA_FORMAT_R8_UINT;
		case SpvImageFormatR64ui:
			return RDC::DATA_FORMAT_R64_UINT;
		case SpvImageFormatR64i:
			return RDC::DATA_FORMAT_R64_SINT;
		case SpvImageFormatMax:
			return RDC::DATA_FORMAT_MAX;
		}
		return RDC::DATA_FORMAT_MAX;
	}

	static RenderingDeviceCommons::PipelineSpecializationConstantType spv_spec_constant_type_from_id(const SpvReflectShaderModule& module, uint32_t spirv_id)
	{
		uint32_t type_id = 0;

		// Iterate through the SPIR-V words to find the OpCode defining this spirv_id
		uint32_t* word_ptr = module._internal->spirv_code;
		uint32_t* end_ptr = word_ptr + module._internal->spirv_word_count;

		//auto type = module._internal->type_descriptions[p_sc->spirv_id].type_flags;

		// Skip the header (5 words)
		word_ptr += 5;

		while (word_ptr < end_ptr) {
			uint32_t word = *word_ptr;
			uint16_t word_count = (uint16_t)(word >> 16);
			uint16_t opcode = (uint16_t)(word & 0xFFFF);

			// OpSpecConstant, OpSpecConstantTrue, OpSpecConstantFalse, etc.
			// These all have the Result ID at Word 2 and Type ID at Word 1
			if (opcode >= 43 && opcode <= 51) {
				uint32_t result_id = word_ptr[2];
				if (result_id == spirv_id) {
					type_id = word_ptr[1];
					break;
				}
			}
			word_ptr += word_count;
		}

		// Now look up that type_id in the internal type_descriptions list
		if (type_id != 0) {
			for (size_t t = 0; t < module._internal->type_description_count; ++t) {
				SpvReflectTypeDescription* td = &module._internal->type_descriptions[t];
				if (td->id == type_id) {
					// SUCCESS: You now have the type details (float, int, etc.)
					SpvReflectTypeFlagBits flag = (SpvReflectTypeFlagBits)td->type_flags;
					switch (flag)
					{
					case SPV_REFLECT_TYPE_FLAG_BOOL:

						return RenderingDeviceCommons::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
						break;
					case SPV_REFLECT_TYPE_FLAG_INT:
						return RenderingDeviceCommons::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
						break;
					case SPV_REFLECT_TYPE_FLAG_FLOAT:
						return 	RenderingDeviceCommons::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT;
						break;
					default:
						break;
					}
				}
			}
		}
	}

	bool RenderingShaderContainer::compress_code(const uint8_t* p_decompressed_bytes, uint32_t p_decompressed_size, uint8_t* p_compressed_bytes, uint32_t* r_compressed_size, uint32_t* r_compressed_flags) const
	{
		// No compressiong support yet.

		return true;
	}
	bool RenderingShaderContainer::decompress_code(const uint8_t* p_compressed_bytes, uint32_t p_compressed_size, uint32_t p_compressed_flags, uint8_t* p_decompressed_bytes, uint32_t p_decompressed_size) const
	{
		// No compressiong support yet.
		return true;
	}

	RenderingShaderContainer::RenderingShaderContainer()
	{
	}

	RenderingShaderContainer::~RenderingShaderContainer()
	{
	}

	RenderingDeviceCommons::ShaderReflection RenderingShaderContainer::get_shader_reflection() const
	{
		RDC::ShaderReflection shader_refl;
		shader_refl.push_constant_size = reflection_data.push_constant_size;
		shader_refl.push_constant_stages = reflection_data.push_constant_stages_mask;
		shader_refl.vertex_input_mask = reflection_data.vertex_input_mask;
		shader_refl.fragment_output_mask = reflection_data.fragment_output_mask;
		shader_refl.pipeline_type = reflection_data.pipeline_type;
		shader_refl.has_multiview = reflection_data.has_multiview;
		shader_refl.has_dynamic_buffers = reflection_data.has_dynamic_buffers;
		shader_refl.compute_local_size[0] = reflection_data.compute_local_size[0];
		shader_refl.compute_local_size[1] = reflection_data.compute_local_size[1];
		shader_refl.compute_local_size[2] = reflection_data.compute_local_size[2];
		shader_refl.uniform_sets.resize(reflection_data.set_count);
		shader_refl.specialization_constants.resize(reflection_data.specialization_constants_count);
		shader_refl.stages_vector.resize(reflection_data.stage_count);

		DEV_ASSERT(reflection_binding_set_uniforms_count.size() == reflection_data.set_count && "The amount of elements in the reflection and the shader container can't be different.");
		uint32_t uniform_index = 0;
		for (uint32_t i = 0; i < reflection_data.set_count; i++) {
			std::vector<RDC::ShaderUniform>& uniform_set = shader_refl.uniform_sets.data()[i];
			uint32_t uniforms_count = reflection_binding_set_uniforms_count[i];
			uniform_set.resize(uniforms_count);
			for (uint32_t j = 0; j < uniforms_count; j++) {
				const ReflectionBindingData& binding = reflection_binding_set_uniforms_data[uniform_index++];
				RDC::ShaderUniform& uniform = uniform_set.data()[j];
				uniform.type = RDC::UniformType(binding.type);
				uniform.writable = binding.writable;
				uniform.length = binding.length;
				uniform.binding = binding.binding;
				uniform.stages = binding.stages;
			}
		}

		shader_refl.specialization_constants.resize(reflection_data.specialization_constants_count);
		for (uint32_t i = 0; i < reflection_data.specialization_constants_count; i++) {
			const ReflectionSpecializationData& spec = reflection_specialization_data[i];
			RDC::ShaderSpecializationConstant& sc = shader_refl.specialization_constants.data()[i];
			sc.type = RDC::PipelineSpecializationConstantType(spec.type);
			sc.constant_id = spec.constant_id;
			sc.int_value = spec.int_value;
			sc.stages = spec.stage_flags;
		}

		shader_refl.stages_vector.resize(reflection_data.stage_count);
		for (uint32_t i = 0; i < reflection_data.stage_count; i++) {
			shader_refl.stages_vector[i] = reflection_shader_stages[i];
			shader_refl.stages_bits.set_flag(RDC::ShaderStage(1U << reflection_shader_stages[i]));
		}

		return shader_refl;
	}

	bool RenderingShaderContainer::from_shader_stage_spirv_data(std::vector<RenderingDeviceCommons::ShaderStageSPIRVData>& data)
	{
		shaders.resize(data.size());

		for (int i = 0; i < data.size(); i++)
		{
			shaders[i].code_compressed_bytes = data[i].spirv;
			shaders[i].shader_stage = data[i].shader_stage;
		}
		return true;
	}

	bool RenderingShaderContainer::from_bytes(const PackedByteArray& p_bytes)
	{
		// TODO:
		return false;
	}

	PackedByteArray RenderingShaderContainer::to_bytes() const
	{
		// TODO:
		return {};
	}

	void RenderingShaderContainer::_set_from_shader_reflection_post(const ReflectShader& p_shader)
	{
		// Do nothing!
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

	Error RenderingShaderContainer::reflect_spirv(const std::string& p_shader_name, std::span<RDC::ShaderStageSPIRVData> p_spirv, ReflectShader& r_shader)
	{
		ReflectShader& reflection = r_shader;
		shader_name = p_shader_name;

		const uint32_t spirv_size = p_spirv.size() + 0;

		std::vector<ReflectShaderStage>& r_refl = r_shader.shader_stages;
		r_refl.resize(spirv_size);

		bool pipeline_type_detected = false;
		for (uint32_t i = 0; i < spirv_size; i++) {
			RDC::ShaderStage stage = p_spirv[i].shader_stage;
			RDC::ShaderStage stage_flag = (RDC::ShaderStage)(1 << stage);
			r_refl[i].shader_stage = stage;
			r_refl[i]._spirv_data = p_spirv[i].spirv;

			if (!pipeline_type_detected) {
				switch (stage) {
				case RDC::SHADER_STAGE_VERTEX:
				case RDC::SHADER_STAGE_FRAGMENT:
				case RDC::SHADER_STAGE_TESSELATION_CONTROL:
				case RDC::SHADER_STAGE_TESSELATION_EVALUATION:
					r_shader.pipeline_type = RDC::PIPELINE_TYPE_RASTERIZATION;
					break;
				case RDC::SHADER_STAGE_COMPUTE:
					r_shader.pipeline_type = RDC::PIPELINE_TYPE_COMPUTE;
					break;
				case RDC::SHADER_STAGE_RAYGEN:
				case RDC::SHADER_STAGE_ANY_HIT:
				case RDC::SHADER_STAGE_CLOSEST_HIT:
				case RDC::SHADER_STAGE_MISS:
				case RDC::SHADER_STAGE_INTERSECTION:
					r_shader.pipeline_type = RDC::PIPELINE_TYPE_RAYTRACING;
					break;
				default:
					DEV_ASSERT(false && "Unknown shader stage.");
				}

				pipeline_type_detected = true;
			}

			const std::vector<uint64_t>& dynamic_buffers = p_spirv[i].dynamic_buffers;

			if (stage == RDC::SHADER_STAGE_COMPUTE) {
				ERR_FAIL_COND_V_MSG(spirv_size != 1, FAILED,
					"Compute shaders can only receive one stage, dedicated to compute.");
			}
			ERR_FAIL_COND_V_MSG(reflection.stages_bits.has_flag(stage_flag), FAILED,
				std::format("Stage {} submitted more than once.", std::string(RDC::SHADER_STAGE_NAMES[stage])));
			reflection.stages_bits.set_flag(stage_flag);

			{
				SpvReflectShaderModule& module = *r_refl.data()[i]._module;
				const uint8_t* spirv = p_spirv[i].spirv.data();
				SpvReflectResult result = spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, p_spirv[i].spirv.size(), spirv, &module);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed parsing shader.");

				for (uint32_t j = 0; j < module.capability_count; j++) {
					if (module.capabilities[j].value == SpvCapabilityMultiView) {
						reflection.has_multiview = true;
						break;
					}
				}

				if (reflection.is_compute()) {
					reflection.compute_local_size[0] = module.entry_points->local_size.x;
					reflection.compute_local_size[1] = module.entry_points->local_size.y;
					reflection.compute_local_size[2] = module.entry_points->local_size.z;
				}
				uint32_t binding_count = 0;
				result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating descriptor bindings.");

				if (binding_count > 0) {
					// Parse bindings.

					std::vector<SpvReflectDescriptorBinding*> bindings;
					bindings.resize(binding_count);
					result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed getting descriptor bindings.");

					for (uint32_t j = 0; j < binding_count; j++) {
						const SpvReflectDescriptorBinding& binding = *bindings[j];

						ReflectUniform uniform;
						uniform.set_spv_reflect(stage, &binding);

						bool need_array_dimensions = false;
						bool need_block_size = false;
						bool may_be_writable = false;
						bool is_image = false;

						switch (binding.descriptor_type) {
						case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
							uniform.type = RDC::UNIFORM_TYPE_SAMPLER;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
							uniform.type = RDC::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
							uniform.type = RDC::UNIFORM_TYPE_TEXTURE;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
							uniform.type = RDC::UNIFORM_TYPE_IMAGE;
							need_array_dimensions = true;
							may_be_writable = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
							uniform.type = RDC::UNIFORM_TYPE_TEXTURE_BUFFER;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
							uniform.type = RDC::UNIFORM_TYPE_IMAGE_BUFFER;
							need_array_dimensions = true;
							may_be_writable = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
							//TODO: support for dynamic buffers
							//const uint64_t key = ShaderRD::DynamicBuffer::encode(binding.set, binding.binding);
							//if (dynamic_buffers.has(key)) {
							//	uniform.type = UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC;
							//	reflection.has_dynamic_buffers = true;
							//}
							//else {
							uniform.type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
							//}
							need_block_size = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
							//TODO: support for dynamic buffers
							//const uint64_t key = ShaderRD::DynamicBuffer::encode(binding.set, binding.binding);
							//if (dynamic_buffers.has(key)) {
							//	uniform.type = UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC;
							//	reflection.has_dynamic_buffers = true;
							//}
							//else {
							uniform.type = RDC::UNIFORM_TYPE_STORAGE_BUFFER;
							//}
							need_block_size = true;
							may_be_writable = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: {
							ERR_PRINT("Dynamic uniform buffer not supported.");
							continue;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
							ERR_PRINT("Dynamic storage buffer not supported.");
							continue;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
							uniform.type = RDC::UNIFORM_TYPE_INPUT_ATTACHMENT;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
							uniform.type = RDC::UNIFORM_TYPE_ACCELERATION_STRUCTURE;
						} break;
						}

						if (need_array_dimensions) {
							uniform.length = 1;
							for (uint32_t k = 0; k < binding.array.dims_count; k++) {
								uniform.length *= binding.array.dims[k];
							}
						}
						else if (need_block_size) {
							uniform.length = binding.block.size;
						}
						else {
							uniform.length = 0;
						}

						if (may_be_writable) {
							if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
								uniform.writable = !(binding.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
							}
							else {
								uniform.writable = !(binding.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) && !(binding.block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
							}
						}
						else {
							uniform.writable = false;
						}

						if (is_image) {
							uniform.image.format = spv_image_format_to_data_format(binding.image.image_format);
						}

						uniform.binding = binding.binding;
						uint32_t set = binding.set;

						ERR_FAIL_COND_V_MSG(set >= RDC::MAX_UNIFORM_SETS, FAILED,
							std::format(
								"On shader stage '{}', uniform '{}' uses a set ({}) index larger than what is supported ({}).",
								RDC::SHADER_STAGE_NAMES[stage],
								binding.name,
								set,
								RDC::MAX_UNIFORM_SETS
							));

						if (set < (uint32_t)reflection.uniform_sets.size()) {
							// Check if this already exists.
							bool exists = false;
							for (uint32_t k = 0; k < reflection.uniform_sets[set].size(); k++) {
								if (reflection.uniform_sets[set][k].binding == uniform.binding) {
									// Already exists, verify that it's the same type.
									ERR_FAIL_COND_V_MSG(reflection.uniform_sets[set][k].type != uniform.type, FAILED,
										std::format(
											"On shader stage '{}', uniform '{}' trying to reuse location for set={}, binding={} with different uniform type.",
											RDC::SHADER_STAGE_NAMES[stage],
											binding.name,
											set,
											uniform.binding
										));

									// Also, verify that it's the same size.
									ERR_FAIL_COND_V_MSG(reflection.uniform_sets[set][k].length != uniform.length, FAILED,
										std::format(
											"On shader stage '{}', uniform '{}' trying to reuse location for set={}, binding={} with different uniform size.",
											RDC::SHADER_STAGE_NAMES[stage],
											binding.name,
											set,
											uniform.binding
										));

									// Also, verify that it has the same writability.
									ERR_FAIL_COND_V_MSG(reflection.uniform_sets[set][k].writable != uniform.writable, FAILED,
										std::format(
											"On shader stage '{}', uniform '{}' trying to reuse location for set={}, binding={} with different writability.",
											RDC::SHADER_STAGE_NAMES[stage],
											binding.name,
											set,
											uniform.binding
										));

									// Just append stage mask and return.
									reflection.uniform_sets[set][k].stages.set_flag(stage_flag);
									exists = true;
									break;
								}
							}

							if (exists) {
								continue; // Merged.
							}
						}

						uniform.stages.set_flag(stage_flag);

						if (set >= (uint32_t)reflection.uniform_sets.size()) {
							reflection.uniform_sets.resize(set + 1);
						}

						reflection.uniform_sets[set].push_back(uniform);
					}
				}

				{
					// Specialization constants.

					uint32_t sc_count = 0;
					result = spvReflectEnumerateSpecializationConstants(&module, &sc_count, nullptr);
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating specialization constants.");

					if (sc_count) {
						std::vector<SpvReflectSpecializationConstant*> spec_constants;
						spec_constants.resize(sc_count);

						result = spvReflectEnumerateSpecializationConstants(&module, &sc_count, spec_constants.data());
						ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining specialization constants.");

						for (uint32_t j = 0; j < sc_count; j++) {
							int32_t existing = -1;
							ReflectSpecializationConstant sconst;
							SpvReflectSpecializationConstant* spc = spec_constants[j];
							sconst.set_spv_reflect(stage, spc);

							//if (spc->default_value_size != 4) {
							//	ERR_FAIL_V_MSG(FAILED, std::format("Reflection of SPIR-V shader stage '%s' failed because the specialization constant #%d's default value is not 4 bytes long (%d) and is currently not supported.", SHADER_STAGE_NAMES[p_spirv[i].shader_stage], spc->constant_id, spc->default_value_size));
							//}

							sconst.constant_id = spc->constant_id;
							sconst.int_value = 0; // Clear previous value JIC.

							sconst.type = spv_spec_constant_type_from_id(module, spc->spirv_id);
							switch (sconst.type)
							{
								// TODO: proper default value
							case SPV_REFLECT_TYPE_FLAG_BOOL:
								sconst.bool_value = *(uint32_t*)(false);
								break;
							case SPV_REFLECT_TYPE_FLAG_INT:
								sconst.int_value = *(uint32_t*)(0);
								break;
							case SPV_REFLECT_TYPE_FLAG_FLOAT:
								sconst.float_value = *(float*)(0);
								break;
							default:
								break;
							}


							sconst.stages.set_flag(stage_flag);

							for (uint32_t k = 0; k < reflection.specialization_constants.size(); k++) {
								if (reflection.specialization_constants[k].constant_id == sconst.constant_id) {
									ERR_FAIL_COND_V_MSG(reflection.specialization_constants[k].type != sconst.type, FAILED, std::format("More than one specialization constant used for id ( {} ), but their types differ.", std::to_string(sconst.constant_id)));
									ERR_FAIL_COND_V_MSG(reflection.specialization_constants[k].int_value != sconst.int_value, FAILED, std::format("More than one specialization constant used for id ( {} ), but their default values differ.", std::to_string(sconst.constant_id)));
									existing = k;
									break;
								}
							}

							if (existing >= 0) {
								reflection.specialization_constants[existing].stages.set_flag(stage_flag);
							}
							else {
								reflection.specialization_constants.push_back(sconst);
							}
						}

						std::sort(reflection.specialization_constants.begin(), reflection.specialization_constants.end());
					}
				}

				if (stage == RDC::SHADER_STAGE_VERTEX || stage == RDC::SHADER_STAGE_FRAGMENT) {
					uint32_t iv_count = 0;
					result = spvReflectEnumerateInputVariables(&module, &iv_count, nullptr);
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating input variables.");

					if (iv_count) {
						std::vector<SpvReflectInterfaceVariable*> input_vars;
						input_vars.resize(iv_count);

						result = spvReflectEnumerateInputVariables(&module, &iv_count, input_vars.data());
						ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining input variables.");

						for (const SpvReflectInterfaceVariable* v : input_vars) {
							if (!v) {
								continue;
							}
							if (stage == RDC::SHADER_STAGE_VERTEX) {
								if (v->decoration_flags == 0) { // Regular input.
									reflection.vertex_input_mask |= (((uint64_t)1) << v->location);
								}
							}
							if (v->built_in == SpvBuiltInViewIndex) {
								reflection.has_multiview = true;
							}
						}
					}
				}

				if (stage == RDC::SHADER_STAGE_FRAGMENT) {
					uint32_t ov_count = 0;
					result = spvReflectEnumerateOutputVariables(&module, &ov_count, nullptr);
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating output variables.");

					if (ov_count) {
						std::vector<SpvReflectInterfaceVariable*> output_vars;
						output_vars.resize(ov_count);

						result = spvReflectEnumerateOutputVariables(&module, &ov_count, output_vars.data());
						ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining output variables.");

						for (const SpvReflectInterfaceVariable* refvar : output_vars) {
							if (!refvar) {
								continue;
							}
							if (refvar->built_in != SpvBuiltInFragDepth) {
								reflection.fragment_output_mask |= 1 << refvar->location;
							}
						}
					}
				}

				uint32_t pc_count = 0;
				result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating push constants.");

				if (pc_count) {
					ERR_FAIL_COND_V_MSG(pc_count > 1, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Only one push constant is supported, which should be the same across shader stages.");

					std::vector<SpvReflectBlockVariable*> pconstants;
					pconstants.resize(pc_count);
					result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, pconstants.data());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining push constants.");
#if 0
					if (pconstants[0] == nullptr) {
						Ref<FileAccess> f = FileAccess::open("res://popo.spv", FileAccess::WRITE);
						f->store_buffer((const uint8_t*)&SpirV[0], SpirV.size() * sizeof(uint32_t));
					}
#endif

					ERR_FAIL_COND_V_MSG(reflection.push_constant_size && reflection.push_constant_size != pconstants[0]->size, FAILED,
						"Reflection of SPIR-V shader stage '" + std::string(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Push constant block must be the same across shader stages.");

					reflection.push_constant_size = pconstants[0]->size;
					reflection.push_constant_stages.set_flag(stage_flag);

					//print_line("Stage: " + std::string(SHADER_STAGE_NAMES[stage]) + " push constant of size=" + itos(push_constant.push_constant_size));
				}
			}
		}

		// Sort all uniform_sets by binding.
		for (uint32_t i = 0; i < reflection.uniform_sets.size(); i++) {
			std::sort(reflection.uniform_sets[i].begin(), reflection.uniform_sets[i].end());
		}

		set_from_shader_reflection(reflection);

		return OK;

	}

	bool RenderingShaderContainer::set_code_from_spirv(const std::string& p_shader_name, std::span<RDC::ShaderStageSPIRVData> p_spirv) {
		ReflectShader shader;
		ERR_FAIL_COND_V(reflect_spirv(p_shader_name, p_spirv, shader) != OK, false);
		return _set_code_from_spirv(shader);
	}

	template <class T>
	const T& RenderingShaderContainer::ReflectSymbol<T>::get_spv_reflect(RDC::ShaderStage p_stage) const {
		const T* info = _spv_reflect[get_index_for_stage(p_stage)];
		DEV_ASSERT(info != nullptr); // Caller is expected to specify valid shader stages
		return *info;
	}

	template <class T>
	void RenderingShaderContainer::ReflectSymbol<T>::set_spv_reflect(RDC::ShaderStage p_stage, const T* p_spv) {
		stages.set_flag(1 << p_stage);
		_spv_reflect[get_index_for_stage(p_stage)] = p_spv;
	}

	RenderingShaderContainer::ReflectShaderStage::ReflectShaderStage() {
		_module = new SpvReflectShaderModule;
		memset(_module, 0, sizeof(SpvReflectShaderModule));
	}

	RenderingShaderContainer::ReflectShaderStage::~ReflectShaderStage() {
		spvReflectDestroyShaderModule(_module);
		delete _module;
		_module = nullptr;
	}

	const SpvReflectShaderModule& RenderingShaderContainer::ReflectShaderStage::module() const {
		return *_module;
	}

	const std::span<const uint32_t> RenderingShaderContainer::ReflectShaderStage::spirv() const {
		return { reinterpret_cast<const uint32_t*>(_spirv_data.data()),
				 _spirv_data.size() / sizeof(uint32_t) };
	}
}
