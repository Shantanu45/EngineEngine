#pragma once
#include <cstdint>
#include <vector>
#include "vulkan_common.h"
#include "util/bit_field.h"
#include "util/error_macros.h"
#include <span>
#include <string>

using PackedByteArray = std::vector<uint8_t>;

struct SpvReflectShaderModule;
struct SpvReflectDescriptorBinding;
struct SpvReflectSpecializationConstant;
namespace Vulkan
{

	enum UniformType {
		UNIFORM_TYPE_SAMPLER, // For sampling only (sampler GLSL type).
		UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, // For sampling only, but includes a texture, (samplerXX GLSL type), first a sampler then a texture.
		UNIFORM_TYPE_TEXTURE, // Only texture, (textureXX GLSL type).
		UNIFORM_TYPE_IMAGE, // Storage image (imageXX GLSL type), for compute mostly.
		UNIFORM_TYPE_TEXTURE_BUFFER, // Buffer texture (or TBO, textureBuffer type).
		UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER, // Buffer texture with a sampler(or TBO, samplerBuffer type).
		UNIFORM_TYPE_IMAGE_BUFFER, // Texel buffer, (imageBuffer type), for compute mostly.
		UNIFORM_TYPE_UNIFORM_BUFFER, // Regular uniform buffer (or UBO).
		UNIFORM_TYPE_STORAGE_BUFFER, // Storage buffer ("buffer" qualifier) like UBO, but supports storage, for compute mostly.
		UNIFORM_TYPE_INPUT_ATTACHMENT, // Used for sub-pass read/write, for mobile mostly.
		UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC, // Same as UNIFORM but created with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT.
		UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC, // Same as STORAGE but created with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT.
		UNIFORM_TYPE_ACCELERATION_STRUCTURE, // Bounding Volume Hierarchy (Top + Bottom Level acceleration structures), for raytracing only.
		UNIFORM_TYPE_MAX
	};


	struct ShaderStageSPIRVData {
		ShaderStage shader_stage = SHADER_STAGE_MAX;
		std::vector<uint8_t> spirv;
		std::vector<uint64_t> dynamic_buffers;
	};


	struct ShaderUniform {
		UniformType type = UniformType::UNIFORM_TYPE_MAX;
		bool writable = false;
		uint32_t binding = 0;
		BitField<ShaderStage> stages = {};
		uint32_t length = 0; // Size of arrays (in total elements), or ubos (in bytes * total elements).

		bool operator!=(const ShaderUniform& p_other) const {
			return binding != p_other.binding || type != p_other.type || writable != p_other.writable || stages != p_other.stages || length != p_other.length;
		}

		bool operator<(const ShaderUniform& p_other) const {
			if (binding != p_other.binding) {
				return binding < p_other.binding;
			}
			if (type != p_other.type) {
				return type < p_other.type;
			}
			if (writable != p_other.writable) {
				return writable < p_other.writable;
			}
			if (stages != p_other.stages) {
				return stages < p_other.stages;
			}
			if (length != p_other.length) {
				return length < p_other.length;
			}
			return false;
		}
	};

	enum PipelineSpecializationConstantType {
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL,
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT,
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT,
	};

	struct PipelineSpecializationConstant {
		PipelineSpecializationConstantType type = {};
		uint32_t constant_id = 0xffffffff;
		union {
			uint32_t int_value = 0;
			float float_value;
			bool bool_value;
		};
	};

	struct ShaderSpecializationConstant : public PipelineSpecializationConstant {
		BitField<ShaderStage> stages = {};

		bool operator<(const ShaderSpecializationConstant& p_other) const { return constant_id < p_other.constant_id; }
	};

	struct ShaderReflection {
		uint64_t vertex_input_mask = 0;
		uint32_t fragment_output_mask = 0;
		PipelineType pipeline_type = PIPELINE_TYPE_RASTERIZATION;
		bool has_multiview = false;
		bool has_dynamic_buffers = false;
		uint32_t compute_local_size[3] = {};
		uint32_t push_constant_size = 0;

		std::vector<std::vector<ShaderUniform>> uniform_sets;
		std::vector<ShaderSpecializationConstant> specialization_constants;
		std::vector<ShaderStage> stages_vector;
		BitField<ShaderStage> stages_bits = {};
		BitField<ShaderStage> push_constant_stages = {};
	};

	const char* SHADER_STAGE_NAMES[SHADER_STAGE_MAX] = {
	"Vertex",
	"Fragment",
	"TesselationControl",
	"TesselationEvaluation",
	"Compute",
	};

	class RenderingShaderContainer 
	{

	public:
		static const uint32_t CONTAINER_MAGIC_NUMBER = 0x43535247;
		static const uint32_t CONTAINER_VERSION = 2;
		static const uint32_t FORMAT_VERSION;
		static const uint32_t MAX_UNIFORM_SETS = 16;


		bool debug_info_enabled = false;

		enum CompressionFlagsVulkan {
			COMPRESSION_FLAG_SMOLV = 0x10000,
		};

		
	protected:
		//using Device = Device;

		struct ContainerHeader {
			uint32_t magic_number = 0;
			uint32_t version = 0;
			uint32_t format = 0;
			uint32_t format_version = 0;
			uint32_t shader_count = 0;
		};

		struct ReflectionData {
			uint64_t vertex_input_mask = 0;
			uint32_t fragment_output_mask = 0;
			uint32_t specialization_constants_count = 0;
			PipelineType pipeline_type = PIPELINE_TYPE_RASTERIZATION;
			uint32_t has_multiview = 0;
			uint32_t has_dynamic_buffers = 0;
			uint32_t compute_local_size[3] = {};
			uint32_t set_count = 0;
			uint32_t push_constant_size = 0;
			uint32_t push_constant_stages_mask = 0;
			uint32_t stage_count = 0;
			uint32_t shader_name_len = 0;
		};

		struct ReflectionBindingData {
			uint32_t type = 0;
			uint32_t binding = 0;
			uint32_t stages = 0;
			uint32_t length = 0; // Size of arrays (in total elements), or UBOs (in bytes * total elements).
			uint32_t writable = 0;

			bool operator<(const ReflectionBindingData& p_other) const {
				return binding < p_other.binding;
			}
		};

		struct ReflectionSpecializationData {
			uint32_t type = 0;
			uint32_t constant_id = 0;
			uint32_t int_value = 0;
			uint32_t stage_flags = 0;
		};

		struct ShaderHeader {
			uint32_t shader_stage = 0;
			uint32_t code_compressed_size = 0;
			uint32_t code_compression_flags = 0;
			uint32_t code_decompressed_size = 0;
		};

		ReflectionData reflection_data;
		std::vector<uint32_t> reflection_binding_set_uniforms_count;
		std::vector<ReflectionBindingData> reflection_binding_set_uniforms_data;
		std::vector<ReflectionSpecializationData> reflection_specialization_data;
		std::vector<ShaderStage> reflection_shader_stages;

		virtual uint32_t _format() const;
		virtual uint32_t _format_version() const;

		// These methods will always be called with a valid pointer.
		//virtual uint32_t _from_bytes_header_extra_data(const uint8_t* p_bytes);
		//virtual uint32_t _from_bytes_reflection_extra_data(const uint8_t* p_bytes);
		//virtual uint32_t _from_bytes_reflection_binding_uniform_extra_data_start(const uint8_t* p_bytes);
		//virtual uint32_t _from_bytes_reflection_binding_uniform_extra_data(const uint8_t* p_bytes, uint32_t p_index);
		//virtual uint32_t _from_bytes_reflection_specialization_extra_data_start(const uint8_t* p_bytes);
		//virtual uint32_t _from_bytes_reflection_specialization_extra_data(const uint8_t* p_bytes, uint32_t p_index);
		//virtual uint32_t _from_bytes_shader_extra_data_start(const uint8_t* p_bytes);
		//virtual uint32_t _from_bytes_shader_extra_data(const uint8_t* p_bytes, uint32_t p_index);
		//virtual uint32_t _from_bytes_footer_extra_data(const uint8_t* p_bytes);

		// These methods will be called with a nullptr to retrieve the size of the data.
		//virtual uint32_t _to_bytes_header_extra_data(uint8_t* p_bytes) const;
		//virtual uint32_t _to_bytes_reflection_extra_data(uint8_t* p_bytes) const;
		//virtual uint32_t _to_bytes_reflection_binding_uniform_extra_data(uint8_t* p_bytes, uint32_t p_index) const;
		//virtual uint32_t _to_bytes_reflection_specialization_extra_data(uint8_t* p_bytes, uint32_t p_index) const;
		//virtual uint32_t _to_bytes_shader_extra_data(uint8_t* p_bytes, uint32_t p_index) const;
		//virtual uint32_t _to_bytes_footer_extra_data(uint8_t* p_bytes) const;

		template <class T>
		struct ReflectSymbol {
			static constexpr uint32_t STAGE_INDEX[SHADER_STAGE_MAX] = {
				0, // SHADER_STAGE_VERTEX
				1, // SHADER_STAGE_FRAGMENT
				0, // SHADER_STAGE_TESSELATION_CONTROL
				1, // SHADER_STAGE_TESSELATION_EVALUATION
				0, // SHADER_STAGE_COMPUTE
			};

			BitField<ShaderStage> stages = {};

		private:
			const T* _spv_reflect[2] = { nullptr };

		public:
			_FORCE_INLINE_ constexpr uint32_t get_index_for_stage(ShaderStage p_stage) const {
				DEV_ASSERT(stages.has_flag((1 << p_stage)));
				return STAGE_INDEX[p_stage];
			}

			const T& get_spv_reflect(ShaderStage p_stage) const;

			/*! Returns the first valid stage if multiple stages are set.
			 *
			 * Crashes if no stages are set.
			 */
			const T& get_spv_reflect() const {
				for (const T* d : _spv_reflect) {
					if (d != nullptr) {
						return *d;
					}
				}
				CRASH_NOW_MSG("No stages set in ReflectSymbol");
			}
			void set_spv_reflect(ShaderStage p_stage, const T* p_spv);
		};

		struct ReflectImageTraits {
			DataFormat format = DATA_FORMAT_MAX;
		};

		struct ReflectUniform : ReflectSymbol<SpvReflectDescriptorBinding> {
			UniformType type = UniformType::UNIFORM_TYPE_MAX;
			uint32_t binding = 0;

			ReflectImageTraits image;

			uint32_t length = 0; // Size of arrays (in total elements), or ubos (in bytes * total elements).
			bool writable = false;

			bool operator<(const ReflectUniform& p_other) const {
				if (binding != p_other.binding) {
					return binding < p_other.binding;
				}
				if (type != p_other.type) {
					return type < p_other.type;
				}
				if (writable != p_other.writable) {
					return writable < p_other.writable;
				}
				if (stages != p_other.stages) {
					return stages < p_other.stages;
				}
				if (length != p_other.length) {
					return length < p_other.length;
				}
				return false;
			}
		};

		struct ReflectSpecializationConstant : ReflectSymbol<SpvReflectSpecializationConstant> {
			PipelineSpecializationConstantType type = {};
			uint32_t constant_id = 0xffffffff;
			union {
				uint32_t int_value = 0;
				float float_value;
				bool bool_value;
			};

			bool operator<(const ReflectSpecializationConstant& p_other) const { return constant_id < p_other.constant_id; }
		};

		class ReflectShaderStage {
			friend class RenderingShaderContainer;

			std::vector<uint8_t> _spirv_data;
			SpvReflectShaderModule* _module = nullptr;

		public:
			ShaderStage shader_stage = SHADER_STAGE_MAX;
			const SpvReflectShaderModule& module() const;
			const std::span<uint32_t> spirv() const;
			const std::vector<uint8_t> spirv_data() const { return _spirv_data; }

			ReflectShaderStage();
			~ReflectShaderStage();
		};

		typedef std::vector<ReflectUniform> ReflectDescriptorSet;

		struct ReflectShader {
			uint64_t vertex_input_mask = 0;
			uint32_t fragment_output_mask = 0;
			uint32_t compute_local_size[3] = {};
			uint32_t push_constant_size = 0;
			bool has_multiview = false;
			bool has_dynamic_buffers = false;
			PipelineType pipeline_type = PIPELINE_TYPE_RASTERIZATION;

			std::vector<ReflectShaderStage> shader_stages;
			std::vector<ReflectDescriptorSet> uniform_sets;
			std::vector<ReflectSymbol<SpvReflectDescriptorBinding>> reflect_uniforms;
			std::vector<ReflectSpecializationConstant> specialization_constants;
			std::vector<ReflectSymbol<SpvReflectSpecializationConstant>> reflect_specialization_constants;
			std::vector<ShaderStage> stages_vector;
			BitField<ShaderStage> stages_bits = {};
			BitField<ShaderStage> push_constant_stages = {};

			_FORCE_INLINE_ bool is_compute() const {
				return stages_bits.has_flag(SHADER_STAGE_COMPUTE_BIT);
			}

			/*! Returns the uniform at the specified global index.
			 *
			 * This is a flattened view of all uniform sets.
			 */
			ReflectUniform& uniform_at(uint32_t p_index) {
				for (std::vector<ReflectUniform>& set : uniform_sets) {
					if (p_index < set.size()) {
						return set[p_index];
					}
					p_index -= set.size();
				}
				CRASH_NOW_MSG(std::format("Uniform index %d out of range (total %d)", p_index, uniform_count()));
			}

			uint32_t uniform_count() const {
				uint32_t count = 0;
				for (const std::vector<ReflectUniform>& set : uniform_sets) {
					count += set.size();
				}
				return count;
			}
		};

		// This method will be called when set_from_shader_reflection() is finished. Used to update internal structures to match the reflection if necessary.
		virtual void _set_from_shader_reflection_post(const ReflectShader& p_shader);

		// This method will be called when set_code_from_spirv() is called.
		virtual bool _set_code_from_spirv(const ReflectShader& p_shader);

		void set_from_shader_reflection(const ReflectShader& p_reflection);
		Error reflect_spirv(const std::string& p_shader_name, std::span<ShaderStageSPIRVData> p_spirv, ReflectShader& r_shader);

	public:
		enum CompressionFlags {
			COMPRESSION_FLAG_ZSTD = 0x1,
		};

		struct Shader {
			ShaderStage shader_stage = SHADER_STAGE_MAX;
			PackedByteArray code_compressed_bytes;
			uint32_t code_compression_flags = 0;
			uint32_t code_decompressed_size = 0;
		};

		std::string shader_name;
		std::vector<Shader> shaders;

		bool set_code_from_spirv(const std::string& p_shader_name, std::vector<ShaderStageSPIRVData> p_spirv);
		ShaderReflection get_shader_reflection() const;
		bool from_bytes(const PackedByteArray& p_bytes);
		PackedByteArray to_bytes() const;
		bool compress_code(const uint8_t* p_decompressed_bytes, uint32_t p_decompressed_size, uint8_t* p_compressed_bytes, uint32_t* r_compressed_size, uint32_t* r_compressed_flags) const;
		bool decompress_code(const uint8_t* p_compressed_bytes, uint32_t p_compressed_size, uint32_t p_compressed_flags, uint8_t* p_decompressed_bytes, uint32_t p_decompressed_size) const;
		RenderingShaderContainer();
		RenderingShaderContainer(bool p_debug_info_enabled);
		virtual ~RenderingShaderContainer() = default;
	};

	class RenderingShaderContainerFormatVulkan {
	private:
		bool debug_info_enabled = false;

	public:
		virtual RenderingShaderContainer* create_container() const;
		virtual ShaderLanguageVersion get_shader_language_version() const;
		virtual ShaderSpirvVersion get_shader_spirv_version() const;
		void set_debug_info_enabled(bool p_debug_info_enabled);
		bool get_debug_info_enabled() const;
		RenderingShaderContainerFormatVulkan();
		virtual ~RenderingShaderContainerFormatVulkan();
	};
}
