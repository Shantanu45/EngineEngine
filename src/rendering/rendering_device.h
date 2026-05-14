/*****************************************************************//**
 * \file   rendering_device.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include "rendering_device_commons.h"
#include "rendering_device_driver.h"
#include "rendering_context_driver.h"
#include "util/rid_owner.h"
#include "util/small_vector.h"
#include "xxhash.h"
#include "compiler/compiler.h"
#include "libassert/assert.hpp"

#include <map>
#include <set>

namespace Vulkan
{
	class ImGuiDevice;
}

namespace Rendering
{
	class FramebufferCache;
	class TransientTextureCache;
	static Compiler::Stage compiler_stage_from_shader_stage(const RenderingDeviceCommons::ShaderStage stage)
	{
		switch (stage)
		{
		case RenderingDeviceCommons::SHADER_STAGE_VERTEX:
			return Compiler::Stage::Vertex;
		case RenderingDeviceCommons::SHADER_STAGE_FRAGMENT:
			return Compiler::Stage::Fragment;
		case RenderingDeviceCommons::SHADER_STAGE_TESSELATION_CONTROL:
			return Compiler::Stage::TessControl;
		case RenderingDeviceCommons::SHADER_STAGE_TESSELATION_EVALUATION:
			return Compiler::Stage::TessEvaluation;
		case RenderingDeviceCommons::SHADER_STAGE_COMPUTE:
			return Compiler::Stage::Compute;
		default:
			return Compiler::Stage::Unknown;
		}
	}

	static RenderingDeviceCommons::ShaderStage shader_stage_from_compiler_stage(const Compiler::Stage stage)
	{
		switch (stage)
		{
		case Compiler::Stage::Vertex:
			return RenderingDeviceCommons::SHADER_STAGE_VERTEX;
		case Compiler::Stage::TessControl:
			return RenderingDeviceCommons::SHADER_STAGE_TESSELATION_CONTROL;
		case Compiler::Stage::TessEvaluation:
			return RenderingDeviceCommons::SHADER_STAGE_TESSELATION_EVALUATION;
		case Compiler::Stage::Fragment:
			return RenderingDeviceCommons::SHADER_STAGE_FRAGMENT;
		case Compiler::Stage::Compute:
			return RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		case Compiler::Stage::Geometry:
			return RenderingDeviceCommons::SHADER_STAGE_GEOMETRY;
		case Compiler::Stage::Unknown:
		default:
			//TODO;
			return RenderingDeviceCommons::SHADER_STAGE_MAX;
		}
	}

	class RDShaderSource{
		std::string source[RenderingDeviceCommons::SHADER_STAGE_MAX];
		RenderingDeviceCommons::ShaderLanguage language = RenderingDeviceCommons::SHADER_LANGUAGE_GLSL;

	public:
		void set_stage_source(RenderingDeviceCommons::ShaderStage p_stage, const std::string& p_source) {
			ERR_FAIL_INDEX(p_stage, RenderingDeviceCommons::SHADER_STAGE_MAX);
			source[p_stage] = p_source;
		}

		std::string get_stage_source(RenderingDeviceCommons::ShaderStage p_stage) const {
			ERR_FAIL_INDEX_V(p_stage, RenderingDeviceCommons::SHADER_STAGE_MAX, std::string());
			return source[p_stage];
		}

		void set_language(RenderingDeviceCommons::ShaderLanguage p_language) {
			language = p_language;
		}

		RenderingDeviceCommons::ShaderLanguage get_language() const {
			return language;
		}
	};

	//internally holds separate SPIR-V blobs per stage
	class RDShaderSPIRV
	{
		Util::SmallVector<uint8_t> bytecode[RenderingDeviceCommons::SHADER_STAGE_MAX];
		std::string compile_error[RenderingDeviceCommons::SHADER_STAGE_MAX];

	public:
		void set_stage_bytecode(RenderingDeviceCommons::ShaderStage p_stage, const Util::SmallVector<uint8_t>& p_bytecode) {
			ERR_FAIL_INDEX(p_stage, RenderingDeviceCommons::SHADER_STAGE_MAX);
			bytecode[p_stage] = p_bytecode;
		}

		Util::SmallVector<uint8_t> get_stage_bytecode(RenderingDeviceCommons::ShaderStage p_stage) const {
			ERR_FAIL_INDEX_V(p_stage, RenderingDeviceCommons::SHADER_STAGE_MAX, Util::SmallVector<uint8_t>());
			return bytecode[p_stage];
		}

		Util::SmallVector<RenderingDeviceCommons::ShaderStageSPIRVData> get_stages() const {
			Util::SmallVector<RenderingDeviceCommons::ShaderStageSPIRVData> stages;
			for (int i = 0; i < RenderingDeviceCommons::SHADER_STAGE_MAX; i++) {
				if (bytecode[i].size()) {
					RenderingDeviceCommons::ShaderStageSPIRVData stage;
					stage.shader_stage = RenderingDeviceCommons::ShaderStage(i);
					stage.spirv = bytecode[i];
					stages.push_back(stage);
				}
			}
			return stages;
		}

		void set_stage_compile_error(RenderingDeviceCommons::ShaderStage p_stage, const std::string& p_compile_error) {
			ERR_FAIL_INDEX(p_stage, RenderingDeviceCommons::SHADER_STAGE_MAX);
			compile_error[p_stage] = p_compile_error;
		}

		std::string get_stage_compile_error(RenderingDeviceCommons::ShaderStage p_stage) const {
			ERR_FAIL_INDEX_V(p_stage, RenderingDeviceCommons::SHADER_STAGE_MAX, std::string());
			return compile_error[p_stage];
		}
	};

	class RenderingDevice : public RenderingDeviceCommons
	{
	public:
		typedef int64_t DrawListID;
		typedef int64_t FramebufferFormatID;
		typedef int64_t VertexFormatID;
		//base numeric ID for all types
		enum {
			INVALID_FORMAT_ID = -1
		};

		enum VRSMethod {
			VRS_METHOD_NONE,
			VRS_METHOD_FRAGMENT_SHADING_RATE,
			VRS_METHOD_FRAGMENT_DENSITY_MAP,
		};

		enum IDType {
			ID_TYPE_FRAMEBUFFER_FORMAT,
			ID_TYPE_VERTEX_FORMAT,
			ID_TYPE_DRAW_LIST,
			ID_TYPE_COMPUTE_LIST = 4,
			ID_TYPE_RAYTRACING_LIST = 5,
			ID_TYPE_MAX,
			ID_BASE_SHIFT = 58, // 5 bits for ID types.
			ID_MASK = ((int64_t(1) << ID_BASE_SHIFT) - 1),
		};

		enum MemoryType {
			MEMORY_TEXTURES,
			MEMORY_BUFFERS,
			MEMORY_TOTAL
		};

		enum BufferCreationBits {
			BUFFER_CREATION_DEVICE_ADDRESS_BIT = (1 << 0),
			BUFFER_CREATION_AS_STORAGE_BIT = (1 << 1),
			BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT = (1 << 2),
			BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT = (1 << 3),
			BUFFER_CREATION_AS_INDIRECT_BIT = (1 << 4),
		};

		enum StagingRequiredAction {
			STAGING_REQUIRED_ACTION_NONE,
			STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL,
			STAGING_REQUIRED_ACTION_STALL_PREVIOUS,
		};


		struct Uniform {
			UniformType uniform_type = UNIFORM_TYPE_IMAGE;
			uint32_t binding = 0; // Binding index as specified in shader.
			// This flag specifies that this is an immutable sampler to be set when creating pipeline layout.
			bool immutable_sampler = false;

		private:
			// In most cases only one ID is provided per binding, so avoid allocating memory unnecessarily for performance.
			RID id; // If only one is provided, this is used.
			Util::SmallVector<RID> ids; // If multiple ones are provided, this is used instead.

		public:
			_FORCE_INLINE_ uint32_t get_id_count() const {
				return (id.is_valid() ? 1 : ids.size());
			}

			_FORCE_INLINE_ RID get_id(uint32_t p_idx) const {
				if (id.is_valid()) {
					ERR_FAIL_COND_V(p_idx != 0, RID());
					return id;
				}
				else {
					return ids[p_idx];
				}
			}
			_FORCE_INLINE_ void set_id(uint32_t p_idx, RID p_id) {
				if (id.is_valid()) {
					ERR_FAIL_COND(p_idx != 0);
					id = p_id;
				}
				else {
					ids[p_idx] = p_id;
				}
			}

			_FORCE_INLINE_ void append_id(RID p_id) {
				if (ids.empty()) {
					if (id == RID()) {
						id = p_id;
					}
					else {
						ids.push_back(id);
						ids.push_back(p_id);
						id = RID();
					}
				}
				else {
					ids.push_back(p_id);
				}
			}

			_FORCE_INLINE_ void clear_ids() {
				id = RID();
				ids.clear();
			}

			_FORCE_INLINE_ Uniform(UniformType p_type, int p_binding, RID p_id) {
				uniform_type = p_type;
				binding = p_binding;
				id = p_id;
			}
			_FORCE_INLINE_ Uniform(UniformType p_type, int p_binding, const Util::SmallVector<RID>& p_ids) {
				uniform_type = p_type;
				binding = p_binding;
				ids = p_ids;
			}
			_FORCE_INLINE_ Uniform() = default;
		};
		typedef Uniform PipelineImmutableSampler;

		struct VertexDescriptionKey {
			Util::SmallVector<VertexAttribute> vertex_formats;

			bool operator==(const VertexDescriptionKey& p_key) const {
				int vdc = vertex_formats.size();
				int vdck = p_key.vertex_formats.size();

				if (vdc != vdck) {
					return false;
				}
				else {
					const VertexAttribute* a_ptr = vertex_formats.data();
					const VertexAttribute* b_ptr = p_key.vertex_formats.data();
					for (int i = 0; i < vdc; i++) {
						const VertexAttribute& a = a_ptr[i];
						const VertexAttribute& b = b_ptr[i];

						if (a.location != b.location) {
							return false;
						}
						if (a.offset != b.offset) {
							return false;
						}
						if (a.format != b.format) {
							return false;
						}
						if (a.stride != b.stride) {
							return false;
						}
						if (a.frequency != b.frequency) {
							return false;
						}
					}
					return true; // They are equal.
				}
			}

			uint32_t hash() const {
				int vdc = vertex_formats.size();

				XXH32_state_t* state = XXH32_createState();
				XXH32_reset(state, 0);

				XXH32_update(state, &vdc, sizeof(vdc));

				const VertexAttribute* ptr = vertex_formats.data();
				for (int i = 0; i < vdc; i++) {
					const VertexAttribute& vd = ptr[i];

					XXH32_update(state, &vd.location, sizeof(vd.location));
					XXH32_update(state, &vd.offset, sizeof(vd.offset));
					XXH32_update(state, &vd.format, sizeof(vd.format));
					XXH32_update(state, &vd.stride, sizeof(vd.stride));
					XXH32_update(state, &vd.frequency, sizeof(vd.frequency));
				}

				uint32_t h = XXH32_digest(state);
				XXH32_freeState(state);
				return h;
			}
		};

		struct VertexDescriptionHash {
			std::size_t operator()(const VertexDescriptionKey& p_key) const {
				return p_key.hash();
			}
		};

		struct VertexDescriptionCache {
			Util::SmallVector<VertexAttribute> vertex_formats;
			VertexAttributeBindingsMap bindings;
			RDD::VertexFormatID driver_id;
		};

		struct UniformSetFormat {
			Util::SmallVector<ShaderUniform> uniforms;

			_FORCE_INLINE_ bool operator<(const UniformSetFormat& p_other) const {
				if (uniforms.size() != p_other.uniforms.size()) {
					return uniforms.size() < p_other.uniforms.size();
				}
				for (int i = 0; i < uniforms.size(); i++) {
					if (uniforms[i] < p_other.uniforms[i]) {
						return true;
					}
					else if (p_other.uniforms[i] < uniforms[i]) {
						return false;
					}
				}
				return false;
			}
		};

		struct RenderPipeline {
			// Cached values for validation.
#ifdef DEBUG_ENABLED
			struct Validation {
				FramebufferFormatID framebuffer_format;
				uint32_t render_pass = 0;
				uint32_t dynamic_state = 0;
				VertexFormatID vertex_format;
				bool uses_restart_indices = false;
				uint32_t primitive_minimum = 0;
				uint32_t primitive_divisor = 0;
			} validation;
#endif
			// Actual pipeline.
			RID shader;
			RDD::ShaderID shader_driver_id;
			uint32_t shader_layout_hash = 0;
			Util::SmallVector<uint32_t> set_formats;
			RDD::PipelineID driver_id;
			BitField<RDD::PipelineStageBits> stage_bits = {};
			uint32_t push_constant_size = 0;
		};

		struct ComputePipeline {
			RID shader;
			RDD::ShaderID shader_driver_id;
			uint32_t shader_layout_hash = 0;
			Util::SmallVector<uint32_t> set_formats;
			RDD::PipelineID driver_id;
			BitField<RDD::PipelineStageBits> stage_bits = {};
			uint32_t push_constant_size = 0;
		};

		struct Shader : public ShaderReflection {
			std::string name; // Used for debug.
			RDD::ShaderID driver_id;
			uint32_t layout_hash = 0;
			BitField<RDD::PipelineStageBits> stage_bits = {};
			Util::SmallVector<uint32_t> set_formats;
		};

		struct AttachmentFormat {
			enum : uint32_t {
				UNUSED_ATTACHMENT = 0xFFFFFFFF
			};
			DataFormat format;
			TextureSamples samples;
			uint32_t usage_flags;
			RenderingDeviceDriver::AttachmentLoadOp load_op = RenderingDeviceDriver::ATTACHMENT_LOAD_OP_CLEAR;
			AttachmentFormat() {
				format = DATA_FORMAT_R8G8B8A8_UNORM;
				samples = TEXTURE_SAMPLES_1;
				usage_flags = 0;
			}
		};

		struct FramebufferPass {
			Util::SmallVector<int32_t> color_attachments;
			Util::SmallVector<int32_t> input_attachments;
			Util::SmallVector<int32_t> resolve_attachments;
			Util::SmallVector<int32_t> preserve_attachments;
			int32_t depth_attachment = ATTACHMENT_UNUSED;
			int32_t depth_resolve_attachment = ATTACHMENT_UNUSED;
		};

		struct TextureView {
			DataFormat format_override = DATA_FORMAT_MAX; // // Means, use same as format.
			TextureSwizzle swizzle_r = TEXTURE_SWIZZLE_R;
			TextureSwizzle swizzle_g = TEXTURE_SWIZZLE_G;
			TextureSwizzle swizzle_b = TEXTURE_SWIZZLE_B;
			TextureSwizzle swizzle_a = TEXTURE_SWIZZLE_A;

			bool operator==(const TextureView& p_other) const {
				if (format_override != p_other.format_override) {
					return false;
				}
				else if (swizzle_r != p_other.swizzle_r) {
					return false;
				}
				else if (swizzle_g != p_other.swizzle_g) {
					return false;
				}
				else if (swizzle_b != p_other.swizzle_b) {
					return false;
				}
				else if (swizzle_a != p_other.swizzle_a) {
					return false;
				}
				else {
					return true;
				}
			}
		};

		struct FramebufferFormatKey {
			Util::SmallVector<AttachmentFormat> attachments;
			Util::SmallVector<FramebufferPass> passes;
			uint32_t view_count = 1;
			VRSMethod vrs_method = VRS_METHOD_NONE;
			int32_t vrs_attachment = ATTACHMENT_UNUSED;
			Size2i vrs_texel_size;

			template <typename T>
			static int _compare_scalar(const T& p_a, const T& p_b) {
				if (p_a < p_b) {
					return -1;
				}
				if (p_b < p_a) {
					return 1;
				}
				return 0;
			}

			template <typename T>
			static int _compare_vector(const Util::SmallVector<T>& p_a, const Util::SmallVector<T>& p_b) {
				if (p_a.size() != p_b.size()) {
					return p_a.size() < p_b.size() ? -1 : 1;
				}

				for (uint32_t i = 0; i < p_a.size(); i++) {
					int cmp = _compare_scalar(p_a[i], p_b[i]);
					if (cmp != 0) {
						return cmp;
					}
				}

				return 0;
			}

			static int _compare_pass(const FramebufferPass& p_a, const FramebufferPass& p_b) {
				int cmp = _compare_vector(p_a.color_attachments, p_b.color_attachments);
				if (cmp != 0) {
					return cmp;
				}

				cmp = _compare_vector(p_a.input_attachments, p_b.input_attachments);
				if (cmp != 0) {
					return cmp;
				}

				cmp = _compare_vector(p_a.resolve_attachments, p_b.resolve_attachments);
				if (cmp != 0) {
					return cmp;
				}

				cmp = _compare_vector(p_a.preserve_attachments, p_b.preserve_attachments);
				if (cmp != 0) {
					return cmp;
				}

				cmp = _compare_scalar(p_a.depth_attachment, p_b.depth_attachment);
				if (cmp != 0) {
					return cmp;
				}

				return _compare_scalar(p_a.depth_resolve_attachment, p_b.depth_resolve_attachment);
			}

			static int _compare_attachment(const AttachmentFormat& p_a, const AttachmentFormat& p_b) {
				int cmp = _compare_scalar(p_a.format, p_b.format);
				if (cmp != 0) {
					return cmp;
				}

				cmp = _compare_scalar(p_a.samples, p_b.samples);
				if (cmp != 0) {
					return cmp;
				}

				cmp = _compare_scalar(p_a.usage_flags, p_b.usage_flags);
				if (cmp != 0) {
					return cmp;
				}

				return _compare_scalar(p_a.load_op, p_b.load_op);
			}

			static int _compare_pass_vector(const Util::SmallVector<FramebufferPass>& p_a, const Util::SmallVector<FramebufferPass>& p_b) {
				if (p_a.size() != p_b.size()) {
					return p_a.size() < p_b.size() ? -1 : 1;
				}

				for (uint32_t i = 0; i < p_a.size(); i++) {
					int cmp = _compare_pass(p_a[i], p_b[i]);
					if (cmp != 0) {
						return cmp;
					}
				}

				return 0;
			}

			static int _compare_attachment_vector(const Util::SmallVector<AttachmentFormat>& p_a, const Util::SmallVector<AttachmentFormat>& p_b) {
				if (p_a.size() != p_b.size()) {
					return p_a.size() < p_b.size() ? -1 : 1;
				}

				for (uint32_t i = 0; i < p_a.size(); i++) {
					int cmp = _compare_attachment(p_a[i], p_b[i]);
					if (cmp != 0) {
						return cmp;
					}
				}

				return 0;
			}

			bool operator<(const FramebufferFormatKey& p_key) const {
				int cmp = _compare_scalar(vrs_texel_size.x, p_key.vrs_texel_size.x);
				if (cmp != 0) {
					return cmp < 0;
				}

				cmp = _compare_scalar(vrs_texel_size.y, p_key.vrs_texel_size.y);
				if (cmp != 0) {
					return cmp < 0;
				}

				cmp = _compare_scalar(vrs_attachment, p_key.vrs_attachment);
				if (cmp != 0) {
					return cmp < 0;
				}

				cmp = _compare_scalar(vrs_method, p_key.vrs_method);
				if (cmp != 0) {
					return cmp < 0;
				}

				cmp = _compare_scalar(view_count, p_key.view_count);
				if (cmp != 0) {
					return cmp < 0;
				}

				cmp = _compare_pass_vector(passes, p_key.passes);
				if (cmp != 0) {
					return cmp < 0;
				}

				cmp = _compare_attachment_vector(attachments, p_key.attachments);
				if (cmp != 0) {
					return cmp < 0;
				}

				return false;
			}
		};

		struct FramebufferFormat {
			std::pair<const FramebufferFormatKey, FramebufferFormatID>* E;
			RDD::RenderPassID render_pass; // Here for constructing shaders, never used, see section (7.2. Render Pass Compatibility from Vulkan spec).
			Util::SmallVector<TextureSamples> pass_samples;
			uint32_t view_count = 1; // Number of views.
		};

		typedef void (*InvalidationCallback)(void*);
		struct Framebuffer {
			FramebufferFormatID format_id;
			uint32_t storage_mask = 0;
			Util::SmallVector<RID> texture_ids;
			InvalidationCallback invalidated_callback = nullptr;
			void* invalidated_callback_userdata = nullptr;
			//RDG::FramebufferCache* framebuffer_cache = nullptr;
			Size2 size;
			uint32_t view_count;
		};

		struct Buffer {
			RDD::BufferID driver_id;
			uint32_t size = 0;
			BitField<RDD::BufferUsageBits> usage = {};
			//RDG::ResourceTracker* draw_tracker = nullptr;
			int32_t transfer_worker_index = -1;
			uint64_t transfer_worker_operation = 0;
		};

		struct TransferWorker {
			uint32_t index = 0;
			RDD::BufferID staging_buffer;
			uint32_t max_transfer_size = 0;
			uint32_t staging_buffer_size_in_use = 0;
			uint32_t staging_buffer_size_allocated = 0;
			RDD::CommandBufferID command_buffer;
			RDD::CommandPoolID command_pool;
			RDD::FenceID command_fence;
			Util::SmallVector<RDD::TextureBarrier> texture_barriers;
			bool recording = false;
			bool submitted = false;
			std::mutex thread_mutex;
			uint64_t operations_processed = 0;
			uint64_t operations_submitted = 0;
			uint64_t operations_counter = 0;
			std::mutex operations_mutex;
		};

		struct VertexArray {
			RID buffer;
			VertexFormatID description;
			int vertex_count = 0;
			uint32_t max_instances_allowed = 0;

			Util::SmallVector<RDD::BufferID> buffers; // Not owned, just referenced.
			//Util::SmallVector<RDG::ResourceTracker*> draw_trackers; // Not owned, just referenced.
			Util::SmallVector<uint64_t> offsets;
			Util::SmallVector<int32_t> transfer_worker_indices;
			Util::SmallVector<uint64_t> transfer_worker_operations;
			std::unordered_set <RID> untracked_buffers;
		};

		struct IndexBuffer : public Buffer {
			uint32_t max_index = 0; // Used for validation.
			uint32_t index_count = 0;
			IndexBufferFormat format = INDEX_BUFFER_FORMAT_UINT32;
			bool supports_restart_indices = false;
		};

		struct IndexArray {
			uint32_t max_index = 0; // Remember the maximum index here too, for validation.
			RDD::BufferID driver_id; // Not owned, inherited from index buffer.
			//RDG::ResourceTracker* draw_tracker = nullptr; // Not owned, inherited from index buffer.
			uint32_t offset = 0;
			uint32_t indices = 0;
			IndexBufferFormat format = INDEX_BUFFER_FORMAT_UINT32;
			bool supports_restart_indices = false;
			int32_t transfer_worker_index = -1;
			uint64_t transfer_worker_operation = 0;
		};

		struct UniformSet {
			uint32_t format = 0;
			RID shader_id;
			uint32_t shader_set = 0;
			RDD::UniformSetID driver_id;
			struct AttachableTexture {
				uint32_t bind = 0;
				RID texture;
			};

			struct SharedTexture {
				uint32_t writing = 0;
				RID texture;
			};

			Util::SmallVector<AttachableTexture> attachable_textures; // Used for validation.
			//Util::SmallVector<RenderingDeviceCommons::ResourceTracker*> draw_trackers;
			//Util::SmallVector<RenderingDeviceCommons::ResourceUsage> draw_trackers_usage;
			//std::unordered_map<RID, RenderingDeviceCommons::ResourceUsage> untracked_usage;
			Util::SmallVector<SharedTexture> shared_textures_to_update;
			Util::SmallVector<RID> pending_clear_textures;
			InvalidationCallback invalidated_callback = nullptr;
			void* invalidated_callback_userdata = nullptr;
		};


		struct StagingBufferBlock {
			RDD::BufferID driver_id;
			uint64_t frame_used = 0;
			uint32_t fill_amount = 0;
			uint8_t* data_ptr = nullptr;
		};

		struct StagingBuffers {
			Util::SmallVector<StagingBufferBlock> blocks;
			int current = 0;
			uint32_t block_size = 0;
			uint64_t max_size = 0;
			BitField<RDD::BufferUsageBits> usage_bits = {};
			bool used = false;
		};


		struct RecordedBufferCopy {
			RenderingDeviceDriver::BufferID source;
			RenderingDeviceDriver::BufferCopyRegion region;
		};

		struct RecordedBufferToTextureCopy {
			RDD::BufferID from_buffer;
			RDD::BufferTextureCopyRegion region;
		};

		struct Texture {
			struct SharedFallback {
				uint32_t revision = 1;
				RDD::TextureID texture;
				//RDG::ResourceTracker* texture_tracker = nullptr;
				RDD::BufferID buffer;
				//RDG::ResourceTracker* buffer_tracker = nullptr;
				bool raw_reinterpretation = false;
			};

			RDD::TextureID driver_id;

			TextureType type = TEXTURE_TYPE_MAX;
			DataFormat format = DATA_FORMAT_MAX;
			TextureSamples samples = TEXTURE_SAMPLES_MAX;
			TextureSliceType slice_type = TEXTURE_SLICE_MAX;
			Rect2i slice_rect;
			uint32_t width = 0;
			uint32_t height = 0;
			uint32_t depth = 0;
			uint32_t layers = 0;
			uint32_t mipmaps = 0;
			uint32_t usage_flags = 0;
			uint32_t base_mipmap = 0;
			uint32_t base_layer = 0;

			Util::SmallVector<DataFormat> allowed_shared_formats;

			bool is_resolve_buffer = false;
			bool is_discardable = false;
			bool has_initial_data = false;
			bool pending_clear = false;

			BitField<RDD::TextureAspectBits> read_aspect_flags = {};
			BitField<RDD::TextureAspectBits> barrier_aspect_flags = {};
			bool bound = false; // Bound to framebuffer.
			RID owner;

			//RDG::ResourceTracker* draw_tracker = nullptr;
			//HashMap<Rect2i, RDG::ResourceTracker*>* slice_trackers = nullptr;
			SharedFallback* shared_fallback = nullptr;
			int32_t transfer_worker_index = -1;
			uint64_t transfer_worker_operation = 0;

			RDD::TextureSubresourceRange barrier_range() const {
				RDD::TextureSubresourceRange r;
				r.aspect = barrier_aspect_flags;
				r.base_mipmap = base_mipmap;
				r.mipmap_count = mipmaps;
				r.base_layer = base_layer;
				r.layer_count = layers;
				return r;
			}

			TextureFormat texture_format() const {
				TextureFormat tf;
				tf.format = format;
				tf.width = width;
				tf.height = height;
				tf.depth = depth;
				tf.array_layers = layers;
				tf.mipmaps = mipmaps;
				tf.texture_type = type;
				tf.samples = samples;
				tf.usage_bits = usage_flags;
				tf.shareable_formats = allowed_shared_formats;
				tf.is_resolve_buffer = is_resolve_buffer;
				tf.is_discardable = is_discardable;
				return tf;
			}
		};

		struct CommandBufferPool {
			// Provided by RenderingDevice.
			RDD::CommandPoolID pool;

			// Created internally by RenderingDeviceGraph.
			Util::SmallVector<RDD::CommandBufferID> buffers;
			Util::SmallVector<RDD::SemaphoreID> semaphores;
			uint32_t buffers_used = 0;
		};

		struct Frame {
			std::list<Buffer> buffers_to_dispose_of;
			std::list<Texture> textures_to_dispose_of;
			std::list<Framebuffer> framebuffers_to_dispose_of;
			std::list<RDD::SamplerID> samplers_to_dispose_of;
			std::list<Shader> shaders_to_dispose_of;
			std::list<UniformSet> uniform_sets_to_dispose_of;
			std::list<RenderPipeline> render_pipelines_to_dispose_of;
			std::list<ComputePipeline> compute_pipelines_to_dispose_of;

			// The command pool used by the command buffer.
			RenderingDeviceDriver::CommandPoolID command_pool;

			// The command buffer used by the main thread when recording the frame.
			RenderingDeviceDriver::CommandBufferID command_buffer;

			// Signaled by the command buffer submission. Present must wait on this semaphore.
			RenderingDeviceDriver::SemaphoreID semaphore;

			// Signaled by the command buffer submission. Must wait on this fence before beginning command recording for the frame.
			RenderingDeviceDriver::FenceID fence;
			bool fence_signaled = false;

			// Semaphores the frame must wait on before executing the command buffer.
			Util::SmallVector<RenderingDeviceDriver::SemaphoreID> semaphores_to_wait_on;
			//  Swap chains prepared for drawing during the frame that must be presented.
			Util::SmallVector<RenderingDeviceDriver::SwapChainID> swap_chains_to_present;

			// Semaphores the transfer workers can use to wait before rendering the frame.
			// This must have the same size of the transfer worker pool.
			Util::SmallVector<RenderingDeviceDriver::SemaphoreID> transfer_worker_semaphores;

			// Extra command buffer pool used for driver workarounds or to reduce GPU bubbles by
			// splitting the final render pass to the swapchain into its own cmd buffer.
			CommandBufferPool command_buffer_pool;

			// Extra command buffer pool used for driver workarounds or to reduce GPU bubbles by
			// splitting the final render pass to the swapchain into its own cmd buffer.
			//Device::CommandBufferPool command_buffer_pool;

			struct Timestamp {
				std::string description;
				uint64_t value = 0;
			};

			RDD::QueryPoolID timestamp_pool;

			Util::SmallVector<std::string> timestamp_names;
			Util::SmallVector<uint64_t> timestamp_cpu_values;
			uint32_t timestamp_count = 0;
			Util::SmallVector<std::string> timestamp_result_names;
			Util::SmallVector<uint64_t> timestamp_cpu_result_values;
			Util::SmallVector<uint64_t> timestamp_result_values;
			uint32_t timestamp_result_count = 0;
			uint64_t index = 0;
		};

		uint32_t max_timestamp_query_elements = 0;

		struct ScopedDebugMarker
		{
			ScopedDebugMarker(RenderingDevice* p_device, RenderingDeviceDriver::CommandBufferID p_command_buffer, const std::string& p_name, Color p_lable_color) :
				command_buffer(p_command_buffer),
				device(p_device)
			{
				device->get_driver().command_begin_label(command_buffer, p_name.c_str(), p_lable_color);
			};
			~ScopedDebugMarker()
			{
				device->get_driver().command_end_label(command_buffer);
			};

			ScopedDebugMarker(const ScopedDebugMarker&) = delete;
			ScopedDebugMarker& operator=(const ScopedDebugMarker&) = delete;

		private:
			RenderingDevice* device;
			RenderingDeviceDriver::CommandBufferID command_buffer;
		};

	public:

		static RenderingDevice* get_singleton() {
			static RenderingDevice* singleton = new RenderingDevice();
			return singleton;
		};

		/**
		 * Initializes the rendering device, driver queues, per-frame resources, staging buffers, and caches.
		 * If a main window is supplied, its surface is used to select a present-capable queue.
		 */
		Error initialize(RenderingContextDriver* p_context, DisplayServerEnums::WindowID p_main_window = DisplayServerEnums::INVALID_WINDOW_ID);

		/**
		 * Forwards a platform/window event to optional rendering integrations such as ImGui.
		 */
		void on_poll(void* e);

		/**
		 * Stalls outstanding GPU work and releases all resources owned by the rendering device.
		 */
		void finalize();

#pragma region Shader
		/**
		 * Creates or retrieves a cached shader program from a list of stage source file paths.
		 */
		RID create_program(const std::string& p_shader_name, const Util::SmallVector<std::string> programs);

		/**
		 * Compiles all stages present in a shader source object into SPIR-V bytecode.
		 * The caller owns the returned RDShaderSPIRV pointer.
		 */
		RDShaderSPIRV* shader_compile_spirv_from_shader_source(const RDShaderSource* p_source, bool p_allow_cache = true);

		/**
		 * Creates a shader RID from SPIR-V bytecode and records reflection/layout data needed by pipelines.
		 */
		RID shader_create_from_spirv(const RDShaderSPIRV* p_spirv, const std::string& p_shader_name = "");


		/**
		 * Compiles a single shader source file for one stage and optionally reports compiler errors.
		 */
		Util::SmallVector<uint8_t> shader_compile_spirv_from_source_file(ShaderStage p_stage, const std::string& p_source_code_file,
			ShaderLanguage p_language = SHADER_LANGUAGE_GLSL, std::string* r_error = nullptr, bool p_allow_cache = true);

		/**
		 * Creates a shader RID from a prebuilt container, applying immutable sampler bindings where requested.
		 */
		RID shader_create_from_container_with_samplers(RenderingShaderContainer* shader_container, RID p_placeholder, const Util::SmallVector<PipelineImmutableSampler>& p_immutable_samplers);

		/**
		 * Destroys the driver shader modules associated with a shader RID.
		 */
		void shader_destroy_modules(RID p_shader);

		/**
		 * Returns the vertex attribute locations consumed by a shader as a bit mask.
		 */
		uint64_t shader_get_vertex_input_attribute_mask(RID p_shader);

		/**
		 * Looks up a shader RID by debug/container name.
		 */
		RID get_shader_rid(const std::string& p_name)
		{
			DEBUG_ASSERT(shader_name_rid_map.contains(p_name), "shader container does not exists!");
			return shader_name_rid_map[p_name];
		}
#pragma endregion

#pragma region Pipeline
		/**
		 * Creates a graphics pipeline compatible with a framebuffer format and vertex format.
		 */
		RID render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format,
			RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state,
			const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state,
			const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags = 0,
			uint32_t p_for_render_pass = 0,
			const Util::SmallVector<PipelineSpecializationConstant>& p_specialization_constants = Util::SmallVector<PipelineSpecializationConstant>());

		/**
		 * Creates a graphics pipeline using an existing framebuffer RID to resolve the framebuffer format.
		 */
		RID render_pipeline_create_from_frame_buffer(RID p_shader, RID p_framebuffer, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state, const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags /*= 0*/, uint32_t p_for_render_pass /*= 0*/, const Util::SmallVector<PipelineSpecializationConstant>& p_specialization_constants /*= Util::SmallVector<PipelineSpecializationConstant>()*/);

		/**
		 * Returns true when the RID currently refers to a live render pipeline.
		 */
		bool render_pipeline_is_valid(RID p_pipeline);

		/**
		 * Updates or persists the driver pipeline cache.
		 */
		void update_pipeline_cache(bool p_closing = false);

		/**
		 * Creates a compute pipeline from a compute shader.
		 */
		RID compute_pipeline_create(RID p_shader,
			const Util::SmallVector<PipelineSpecializationConstant>& p_specialization_constants = Util::SmallVector<PipelineSpecializationConstant>());

		/**
		 * Returns true when the RID currently refers to a live compute pipeline.
		 */
		bool compute_pipeline_is_valid(RID p_pipeline);

		/**
		 * Binds a compute pipeline, optionally binds uniform sets, then dispatches.
		 * p_uniform_sets may be empty. Push constants must be set separately via set_push_constant.
		 */
		void compute_dispatch(RID p_pipeline, const Util::SmallVector<RID>& p_uniform_sets, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups);

		/**
		 * Binds a compute pipeline, optionally binds uniform sets, then dispatches using an indirect argument buffer.
		 */
		void compute_dispatch_indirect(RID p_pipeline, const Util::SmallVector<RID>& p_uniform_sets, RID p_indirect_buffer, uint64_t p_offset);
#pragma endregion

#pragma region Screen
		/**
		 * Creates the swap chain and tracking state for a window/screen.
		 */
		Error screen_create(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID);

		/**
		 * Flushes in-flight work and resizes the swap chain associated with a window.
		 */
		void on_resize(const DisplayServerEnums::WindowID active_window);

		/**
		 * Acquires the current swap-chain framebuffer and resizes the swap chain if required.
		 */
		Error screen_prepare_for_drawing(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID);

		/**
		 * Returns the current width of a screen swap chain.
		 */
		int screen_get_width(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;

		/**
		 * Returns the current height of a screen swap chain.
		 */
		int screen_get_height(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;

		/**
		 * Returns the display pre-rotation in degrees for the screen surface.
		 */
		int screen_get_pre_rotation_degrees(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;

		/**
		 * Returns the framebuffer format ID used by the screen swap-chain image.
		 */
		FramebufferFormatID screen_get_framebuffer_format(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;

		/**
		 * Returns the swap-chain color space for a prepared screen.
		 */
		ColorSpace screen_get_color_space(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;

		/**
		 * Releases the swap chain and screen framebuffer tracking state.
		 */
		Error screen_free(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID);
#pragma endregion

#pragma region Framebuffer
		/**
		 * Creates or retrieves a stable framebuffer format ID for a single-pass attachment layout.
		 * The returned ID is cached and does not need to be freed.
		 */
		FramebufferFormatID framebuffer_format_create(const Util::SmallVector<AttachmentFormat>& p_format, uint32_t p_view_count = 1, int32_t p_vrs_attachment = -1);

		/**
		 * Creates or retrieves a stable framebuffer format ID for a multipass attachment layout.
		 */
		FramebufferFormatID framebuffer_format_create_multipass(const Util::SmallVector<AttachmentFormat>& p_attachments, const Util::SmallVector<FramebufferPass>& p_passes, 
			uint32_t p_view_count = 1, int32_t p_vrs_attachment = -1);
			
		/**
		 * Returns the sample count used by a framebuffer format pass.
		 */
		RenderingDevice::TextureSamples framebuffer_format_get_texture_samples(FramebufferFormatID p_format, uint32_t p_pass);

		/**
		 * Creates an empty framebuffer RID with no texture attachments.
		 */
		RID framebuffer_create_empty(const Size2i& p_size, TextureSamples p_samples, FramebufferFormatID p_format_check);

		/**
		 * Creates a framebuffer RID from texture attachment RIDs.
		 */
		RID framebuffer_create(const Util::SmallVector<RID>& p_texture_attachments, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);

		/**
		 * Creates a framebuffer whose attachments are loaded instead of cleared at render-pass begin.
		 */
		RID framebuffer_create_load(const Util::SmallVector<RID>& p_texture_attachments);

		/**
		 * Creates a framebuffer RID using an explicit multipass layout.
		 */
		RID framebuffer_create_multipass(const Util::SmallVector<RID>& p_texture_attachments, const Util::SmallVector<FramebufferPass>& p_passes, FramebufferFormatID p_format_check, uint32_t p_view_count);

		/**
		 * Creates or retrieves a framebuffer format ID for an empty render pass.
		 */
		FramebufferFormatID framebuffer_format_create_empty(TextureSamples p_samples = TEXTURE_SAMPLES_1);

#pragma endregion

#pragma region Buffer
		/**
		 * Creates a GPU vertex buffer and optionally uploads initial data through staging.
		 */
		RID vertex_buffer_create(uint32_t p_size_bytes, std::span<uint8_t> p_data = {}, BitField<BufferCreationBits> p_creation_bits = 0);


		/**
		 * Creates a GPU uniform buffer and optionally uploads initial data through staging.
		 */
		RID uniform_buffer_create(uint32_t p_size_bytes, std::span<uint8_t> p_data = {}, BitField<BufferCreationBits> p_creation_bits = 0);

		/**
		 * Creates a GPU storage buffer and optionally uploads initial data through staging.
		 */
		RID storage_buffer_create(uint32_t p_size_bytes, std::span<uint8_t> p_data = {}, BitField<BufferCreationBits> p_creation_bits = 0);

		/**
		 * Creates a driver uniform set for a shader set index from RD uniform bindings.
		 */
		RID uniform_set_create(const std::span<Uniform>& p_uniforms, RID p_shader, uint32_t p_shader_set, bool p_linear_pool = false);

		/**
		 * Returns a cached uniform set matching the bindings, or creates one when no valid cache entry exists.
		 */
		RID uniform_set_get_or_create(const std::span<Uniform>& p_uniforms, RID p_shader, uint32_t p_shader_set, bool p_linear_pool = false);

		/**
		 * Returns true when the RID currently refers to a live uniform set.
		 */
		bool uniform_set_is_valid(RID p_uniform_set);

		/**
		 * Records a GPU-side copy between two buffers.
		 */
		Error buffer_copy(RID p_src_buffer, RID p_dst_buffer, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size);

		/**
		 * Updates a subrange of a buffer using staging or direct upload.
		 * Offset matters when multiple logical payloads are packed into one buffer, like:
				|--Material_UBO--|--Light_UBO--|
				0               64            128.
		 */
		Error buffer_update(RID p_buffer, uint32_t p_offset, uint32_t p_size, const void* p_data, bool p_skip_check = false);

		/**
		 * Reads a subrange of a GPU buffer into CPU memory, flushing GPU work when required.
		 */
		Util::SmallVector<uint8_t> buffer_get_data(RID p_buffer, uint32_t p_offset, uint32_t p_size);

		/**
		 * Records a GPU-side clear of a buffer range.
		 */
		Error buffer_clear(RID p_buffer, uint32_t p_offset, uint32_t p_size);

		/**
		 * Flushes pending transfer-worker operations that affect a buffer.
		 */
		void buffer_flush(RID p_buffer);

		/**
		 * Creates a vertex array binding vertex buffers to a vertex format.
		 */
		RID vertex_array_create(uint32_t p_vertex_count, VertexFormatID p_vertex_format, const Util::SmallVector<RID>& p_src_buffers, const Util::SmallVector<uint64_t>& p_offsets = Util::SmallVector<uint64_t>());

		/**
		 * Creates an index buffer and optionally uploads initial index data.
		 */
		RID index_buffer_create(uint32_t p_index_count, IndexBufferFormat p_format, std::span<uint8_t> p_data = {},
			bool p_use_restart_indices = false, BitField<BufferCreationBits> p_creation_bits = 0);

		/**
		 * Creates an index array view into an index buffer.
		 */
		RID index_array_create(RID p_index_buffer, uint32_t p_index_offset, uint32_t p_index_count);

		/**
		 * Binds a vertex array to the current command buffer.
		 */
		void bind_vertex_array(RID p_vertex_array);

		/**
		 * Binds an index array to the current command buffer.
		 */
		void bind_index_array(RID p_index_array);

		/**
		 * Binds one RD uniform set for a shader set index.
		 */
		void bind_uniform_set(RID p_shader_id, RID p_uniform_set_id, uint32_t set_index);

		/**
		 * Uploads push-constant data for the currently recording command buffer.
		 */
		void set_push_constant(const void* p_data, uint32_t p_data_size, RID p_shader);

		/**
		 * Binds a span of driver uniform sets for batched draw-list execution.
		 */
		void add_draw_list_bind_uniform_sets(RDD::ShaderID p_shader, std::span<RDD::UniformSetID> p_uniform_sets, uint32_t p_first_index, uint32_t p_set_count);

#pragma endregion

#pragma region Texture

		/**
		 * Creates a texel buffer with a typed element format.
		 */
		RID texture_buffer_create(uint32_t p_size_elements, DataFormat p_format, std::span<uint8_t> p_data = {});

		/**
		 * Acquires a transient texture from the cache or creates one matching the requested format/view.
		 */
		RID acquire_texture(const RDD::TextureFormat& p_format, const RenderingDevice::TextureView& p_view, const Util::SmallVector<Util::SmallVector<uint8_t>>& p_data);

		/**
		 * Releases a transient texture back to the cache.
		 */
		void release_texture(RID p_texture);

		/**
		 * Creates a texture RID, optionally uploading mip/layer data.
		 */
		RID texture_create(const TextureFormat& p_format, const TextureView& p_view, const Util::SmallVector<Util::SmallVector<uint8_t>>& p_data = Util::SmallVector<Util::SmallVector<uint8_t>>());

		/**
		 * Creates a texture RID that shares storage with another texture using a different view.
		 */
		RID texture_create_shared(const TextureView& p_view, RID p_with_texture);

		/**
		 * Wraps an externally-created native texture/image handle in an RD texture RID.
		 */
		RID texture_create_from_extension(TextureType p_type, DataFormat p_format, TextureSamples p_samples, BitField<RenderingDevice::TextureUsageBits> p_usage,
			uint64_t p_image, uint64_t p_width, uint64_t p_height, uint64_t p_depth, uint64_t p_layers, uint64_t p_mipmaps = 1);

		/**
		 * Creates a shared texture RID viewing a layer/mipmap slice of an existing texture.
		 */
		RID texture_create_shared_from_slice(const TextureView& p_view, RID p_with_texture, uint32_t p_layer, uint32_t p_mipmap, uint32_t p_mipmaps = 1,
			TextureSliceType p_slice_type = TEXTURE_SLICE_2D, uint32_t p_layers = 0);

		/**
		 * Updates one texture layer using staging.
		 */
		Error texture_update(RID p_texture, uint32_t p_layer, const Util::SmallVector<uint8_t>& p_data);

		/**
		 * Reads back texture data for one layer, flushing GPU work when required.
		 */
		Util::SmallVector<uint8_t> texture_get_data(RID p_texture, uint32_t p_layer); // CPU textures will return immediately, while GPU textures will most likely force a flush
		//Error texture_get_data_async(RID p_texture, uint32_t p_layer, const Callable& p_callback);

		/**
		 * Returns the driver texture handle backing a texture RID.
		 */
		RDD::TextureID texture_id_from_rid(RID texture);

		/**
		 * Creates a sampler object from sampler state.
		 */
		RID sampler_create(const SamplerState& p_state);

		/**
		 * Returns whether a data format supports the requested filtering mode.
		 */
		bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_sampler_filter) const;

		/**
		 * Emits image barriers into a command buffer.
		 */
		void apply_image_barrier(RDD::CommandBufferID p_cmd_buffer, BitField<RenderingDeviceDriver::PipelineStageBits> p_src_stages, BitField<RenderingDeviceDriver::PipelineStageBits> p_dst_stages, std::span<RenderingDeviceDriver::TextureBarrier> p_texture_barriers);

		/**
		 * Emits buffer barriers into a command buffer.
		 */
		void apply_buffer_barrier(RDD::CommandBufferID p_cmd_buffer, BitField<RenderingDeviceDriver::PipelineStageBits> p_src_stages, BitField<RenderingDeviceDriver::PipelineStageBits> p_dst_stages, std::span<RenderingDeviceDriver::BufferBarrier> p_buffer_barriers);

		/**
		 * Emits a buffer barrier for a RID-owned buffer.
		 */
		void apply_buffer_barrier(RDD::CommandBufferID p_cmd_buffer, BitField<RenderingDeviceDriver::PipelineStageBits> p_src_stages, BitField<RenderingDeviceDriver::PipelineStageBits> p_dst_stages,
			RID p_buffer, BitField<RenderingDeviceDriver::BarrierAccessBits> p_src_access, BitField<RenderingDeviceDriver::BarrierAccessBits> p_dst_access,
			uint64_t p_offset = 0, uint64_t p_size = RenderingDeviceDriver::BUFFER_WHOLE_SIZE);

#pragma endregion

#pragma region Frame

		/**
		 * Submits the current frame command buffer chain, optionally presenting on the graphics queue.
		 */
		void execute_chained_cmds(bool p_present_swap_chain,
			RenderingDeviceDriver::FenceID p_draw_fence,
			RenderingDeviceDriver::SemaphoreID p_dst_draw_semaphore_to_signal);

		/**
		 * Ends, submits, advances, and begins the next frame.
		 */
		void swap_buffers(bool p_present);

		/**
		 * Begins rendering to the current swap-chain framebuffer and sets viewport/scissor.
		 */
		bool begin_for_screen(DisplayServerEnums::WindowID p_screen = 0, const Color& p_clear_color = Color());

		/**
		 * Ends rendering for a screen and queues its swap chain for presentation.
		 */
		bool end_for_screen(DisplayServerEnums::WindowID p_screen);

		/**
		 * Frees a driver framebuffer created outside RID ownership.
		 */
		void free_framebuffer(RDD::FramebufferID p_frame_buffer);

		/**
		 * Creates a driver framebuffer from driver texture attachments.
		 */
		RDD::FramebufferID create_framebuffer(RDD::RenderPassID p_render_pass, std::span<RDD::TextureID> p_attachments, uint32_t p_width, uint32_t p_height);

		/**
		 * Creates a driver framebuffer using a cached framebuffer format ID and texture RID attachments.
		 */
		RDD::FramebufferID create_framebuffer_from_format_id(FramebufferFormatID p_format_id, Util::SmallVector<RID> p_attachments, uint32_t p_width, uint32_t p_height);

		/**
		 * Creates a driver framebuffer using an explicit render pass and texture RID attachments.
		 */
		RDD::FramebufferID create_framebuffer_from_render_pass(RDD::RenderPassID p_render_pass, Util::SmallVector<RID> p_attachments, uint32_t p_width, uint32_t p_height, uint32_t p_layers = 1);

		/**
		 * Returns the driver render pass associated with a framebuffer format ID.
		 */
		RDD::RenderPassID render_pass_from_format_id(FramebufferFormatID p_format_id);

		/**
		 * Begins a render pass on a driver framebuffer.
		 */
		bool begin_render_pass(RDD::RenderPassID p_render_pass, RDD::FramebufferID p_frame_buffer, Rect2i p_region, const Color& p_clear_color);

		/**
		 * Begins a render pass using a framebuffer RID and explicit clear values.
		 */
		bool begin_render_pass_from_frame_buffer(RID p_frame_buffer, Rect2i p_region, const std::span<RenderingDeviceDriver::RenderPassClearValue>& p_clear_color);

		/**
		 * Returns the currently recording primary command buffer.
		 */
		RDD::CommandBufferID get_current_command_buffer();

		/**
		 * Records a non-indexed draw command.
		 */
		void render_draw(RenderingDeviceDriver::CommandBufferID p_command_buffer, uint32_t p_vertex_count, uint32_t p_instance_count);

		/**
		 * Records an indexed draw command.
		 */
		void render_draw_indexed(RenderingDeviceDriver::CommandBufferID p_command_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance);

		/**
		 * Submits pending work for execution.
		 */
		void submit();

		/**
		 * Waits for submitted rendering work to complete.
		 */
		void sync();

		/**
		 * Creates a local rendering device instance sharing the same context.
		 */
		RenderingDevice* create_local_device();

		/**
		 * Creates or retrieves a cached vertex format ID for vertex attributes.
		 */
		VertexFormatID vertex_format_create(const Util::SmallVector<VertexAttribute>& p_vertex_descriptions);

		/**
		 * Creates a pipeline compatible with the swap-chain framebuffer for a window.
		 */
		RID create_swapchain_pipeline(DisplayServerEnums::WindowID window, RID p_shader, VertexFormatID p_vertex_format,
			RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state,
			const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state,
			const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags = 0,
			uint32_t p_for_render_pass = 0,
			const Util::SmallVector<PipelineSpecializationConstant>& p_specialization_constants = Util::SmallVector<PipelineSpecializationConstant>());

		/**
		 * Begins recording the current frame command buffer and frees resources safe for this frame.
		 */
		void begin_frame(bool p_presented = false);

		/**
		 * Finishes recording the current frame command buffer and submits pending transfer barriers.
		 */
		void end_frame();

		/**
		 * Binds a render pipeline on a command buffer.
		 */
		void bind_render_pipeline(RDD::CommandBufferID p_command_buffer, RID pipeline);

		/**
		 * Ends the active render pass on a command buffer.
		 */
		void end_render_pass(RDD::CommandBufferID cmd);

		/**
		 * Submits the current frame and presents when requested.
		 */
		void execute_frame(bool p_present);

#pragma endregion

#pragma region ImGui

		/**
		 * Initializes the ImGui Vulkan bridge for the current rendering context.
		 */
		Error iniitialize_imgui_device(WindowPlatformData p_platfform_data, uint32_t p_devince_index = 0, uint32_t swapchain_index = 0);

		/**
		 * Starts a new ImGui frame.
		 */
		void imgui_begin_frame();

		/**
		 * Returns the texture RID used by the ImGui integration.
		 */
		RID get_imgui_texture();

		/**
		 * Records ImGui draw data into a command buffer.
		 */
		void imgui_execute(void* p_draw_data, RDD::CommandBufferID p_command_buffer, RID p_frame_buffer, RDD::PipelineID p_pipeline = RDD::PipelineID());

		/**
		 * Returns the owned ImGui device bridge.
		 */
		Vulkan::ImGuiDevice* get_imgui_device();

#pragma endregion

#pragma region Debug

		/**
		 * Assigns a human-readable debug name to a supported GPU resource.
		 */
		void set_resource_name(RID p_id, const std::string& p_name);

		/**
		 * Inserts a GPU timestamp query with an associated CPU timestamp/name.
		 */
		void capture_timestamp(const std::string& p_name);

		/**
		 * Returns the number of timestamp results captured for the last completed frame.
		 */
		uint32_t get_captured_timestamps_count() const;

		/**
		 * Returns the frame index associated with captured timestamp results.
		 */
		uint64_t get_captured_timestamps_frame() const;

		/**
		 * Returns a captured GPU timestamp value by result index.
		 */
		uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const;

		/**
		 * Returns a captured CPU timestamp value by result index.
		 */
		uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const;

		/**
		 * Returns the name associated with a captured timestamp result.
		 */
		std::string get_captured_timestamp_name(uint32_t p_index) const;

		/**
		 * Exposes the low-level rendering driver used by this device.
		 */
		inline RenderingDeviceDriver& get_driver() const
		{
			return *driver;
		}

#pragma endregion

#pragma region Resource lifetime

		// TODO: #temp
		/**
		 * Submits all active transfer workers and optionally chains their semaphores into a draw command buffer.
		 */
		void _submit_transfer_workers(RDD::CommandBufferID p_draw_command_buffer = RDD::CommandBufferID());

		/**
		 * Emits texture barriers accumulated by transfer workers into the draw command buffer.
		 */
		void _submit_transfer_barriers(RDD::CommandBufferID p_draw_command_buffer);

		/**
		 * Frees resources that directly depend on the given RID.
		 */
		void _free_dependencies_of(RID p_id);

		/**
		 * Public RID disposal entry point; recursively frees dependencies before queueing the resource for deletion.
		 */
		void free_rid(RID p_rid);

#pragma endregion

	private:

#pragma region Buffer helpers

		/**
		 * Ensures a buffer can be safely modified when shared/immutable tracking requires it.
		 */
		bool _buffer_make_mutable(Buffer* p_buffer, RID p_buffer_id);

		/**
		 * Adapter used by call sites that already store initial data in SmallVector form.
		 */
		RID _vertex_buffer_create(uint32_t p_size_bytes, Util::SmallVector<uint8_t>& p_data, BitField<BufferCreationBits> p_creation_bits = 0) {
			return vertex_buffer_create(p_size_bytes, p_data, p_creation_bits);
		}

		/**
		 * Adapter used by call sites that already store initial index data in SmallVector form.
		 */
		RID _index_buffer_create(uint32_t p_index_count, IndexBufferFormat p_format, Util::SmallVector<uint8_t>& p_data,
			bool p_use_restart_indices = false, BitField<BufferCreationBits> p_creation_bits = 0) {
			return index_buffer_create(p_index_count, p_format, p_data, p_use_restart_indices, p_creation_bits);
		}

		/**
		 * Returns the desired swap-chain image count used during creation/resizing.
		 */
		uint32_t _get_swap_chain_desired_count() const;

		/**
		 * Resolves any supported buffer RID owner to its Buffer record.
		 */
		Buffer* _get_buffer_from_owner(RID p_buffer);

		/**
		 * Uploads initial buffer data, using staging when needed by memory type/alignment.
		 */
		Error _buffer_initialize(Buffer* p_buffer, std::span<uint8_t> p_data, uint32_t p_required_align = 32);

#pragma endregion

#pragma region Lifecycle helpers

		/**
		 * Selects queue families and creates graphics, transfer, and present queues.
		 */
		Error _initialize_queues(RenderingContextDriver::SurfaceID p_main_surface);

		/**
		 * Creates per-frame command pools, command buffers, fences, semaphores, and timestamp query state.
		 */
		Error _initialize_frame_data(uint32_t p_frame_count);

		/**
		 * Initializes Tracy GPU profiling resources when Tracy is enabled.
		 */
		void _initialize_tracy();

		/**
		 * Configures upload/download staging pools and creates the initial per-frame staging blocks.
		 */
		Error _initialize_staging_buffers();

		/**
		 * Flushes shader/transient caches before owner-based teardown.
		 */
		void _finalize_cached_resources();

		/**
		 * Frees all still-owned RID resources and reports leaks through the owner helpers.
		 */
		void _finalize_owned_rids();

		/**
		 * Frees per-frame synchronization, command, timestamp, and pending resource queues.
		 */
		void _finalize_frame_data();

		/**
		 * Unmaps and frees staging buffer blocks.
		 */
		void _finalize_staging_buffers();

		/**
		 * Frees cached vertex formats and framebuffer render passes.
		 */
		void _finalize_format_caches();

		/**
		 * Frees all screen swap chains.
		 */
		void _finalize_swap_chains();

		/**
		 * Frees graphics, transfer, and present queues, avoiding double-free for shared queue handles.
		 */
		void _finalize_queues();

		/**
		 * Releases the rendering driver from the context.
		 */
		void _finalize_driver();

#pragma endregion

#pragma region Transfer Worker

		/**
		 * Acquires a transfer worker and reserves staging space for an upload/download operation.
		 */
		TransferWorker* _acquire_transfer_worker(uint32_t p_transfer_size, uint32_t p_required_align, uint32_t& r_staging_offset);

		/**
		 * Marks a transfer worker as available after recording work into it.
		 */
		void _release_transfer_worker(TransferWorker* p_transfer_worker);

		/**
		 * Ends command recording for a transfer worker.
		 */
		void _end_transfer_worker(TransferWorker* p_transfer_worker);

		/**
		 * Submits one transfer worker and optionally signals semaphores for dependent draw work.
		 */
		void _submit_transfer_worker(TransferWorker* p_transfer_worker, std::span<RDD::SemaphoreID> p_signal_semaphores = std::span<RDD::SemaphoreID>());

		/**
		 * Waits for a submitted transfer worker to finish.
		 */
		void _wait_for_transfer_worker(TransferWorker* p_transfer_worker);

		/**
		 * Moves pending texture barriers from a transfer worker into the shared transfer barrier queue.
		 */
		void _flush_barriers_for_transfer_worker(TransferWorker* p_transfer_worker);

		/**
		 * Waits for a specific transfer worker operation if it has not completed yet.
		 */
		void _check_transfer_worker_operation(uint32_t p_transfer_worker_index, uint64_t p_transfer_worker_operation);

		/**
		 * Ensures pending transfer work touching a buffer has completed before unsafe access/free.
		 */
		void _check_transfer_worker_buffer(Buffer* p_buffer);

		/**
		 * Ensures pending transfer work touching a texture has completed before unsafe access/free.
		 */
		void _check_transfer_worker_texture(Texture* p_texture);

		/**
		 * Ensures transfer operations used by a vertex array's buffers have completed.
		 */
		void _check_transfer_worker_vertex_array(VertexArray* p_vertex_array);

		/**
		 * Ensures transfer operations used by an index array's buffer have completed.
		 */
		void _check_transfer_worker_index_array(IndexArray* p_index_array);

		/**
		 * Waits for all transfer workers to finish.
		 */
		void _wait_for_transfer_workers();

		/**
		 * Frees all transfer worker staging buffers, command pools, and fences.
		 */
		void _free_transfer_workers();

#pragma endregion

#pragma region Texture
		/**
		 * Returns the physical layer count for a texture, expanding cube layers to faces.
		 */
		uint32_t _texture_layer_count(Texture* p_texture) const;

		/**
		 * Returns the staging alignment requirement for a texture upload/download.
		 */
		uint32_t _texture_alignment(Texture* p_texture) const;

		/**
		 * Uploads initial data for a texture layer and transitions it to the requested layout.
		 */
		Error _texture_initialize(RID p_texture, uint32_t p_layer, const Util::SmallVector<uint8_t>& p_data, RDD::TextureLayout p_dst_layout, bool p_immediate_flush);

		/**
		 * Ensures shared texture fallback metadata exists when required by the texture.
		 */
		void _texture_check_shared_fallback(Texture* p_texture);

		/**
		 * Updates shared fallback state before reading from or writing to a shared texture.
		 */
		void _texture_update_shared_fallback(RID p_texture_rid, Texture* p_texture, bool p_for_writing);

		/**
		 * Frees auxiliary shared fallback storage owned by a texture.
		 */
		void _texture_free_shared_fallback(Texture* p_texture);

		/**
		 * Copies content between shared texture owners/views when fallback synchronization requires it.
		 */
		void _texture_copy_shared(RID p_src_texture_rid, Texture* p_src_texture, RID p_dst_texture_rid, Texture* p_dst_texture);

		/**
		 * Creates a buffer reinterpretation for texture formats that require buffer access.
		 */
		void _texture_create_reinterpret_buffer(Texture* p_texture);

		/**
		 * Applies pending clear state before a texture is read or otherwise used.
		 */
		void _texture_check_pending_clear(RID p_texture_rid, Texture* p_texture);

		/**
		 * Records a color clear for a texture subresource range.
		 */
		void _texture_clear_color(RID p_texture_rid, Texture* p_texture, const Color& p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers);

		/**
		 * Records a depth/stencil clear for a texture subresource range.
		 */
		void _texture_clear_depth_stencil(RID p_texture_rid, Texture* p_texture, float p_depth, uint8_t p_stencil, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers);

		/**
		 * Converts the active variable-rate-shading method into texture usage bits.
		 */
		uint32_t _texture_vrs_method_to_usage_bits() const;

		/**
		 * Ensures a texture exposes a format that can be shared with compatible views.
		 */
		void _texture_ensure_shareable_format(RID p_texture, const DataFormat& p_shareable_format);
#pragma endregion

#pragma region Pipeline cache

		/**
		 * Loads serialized pipeline cache bytes from persistent storage.
		 */
		Util::SmallVector<uint8_t> _load_pipeline_cache();

		/**
		 * Persists serialized pipeline cache data.
		 */
		static void _save_pipeline_cache(void* p_data);

#pragma endregion

#pragma region Staging

		/**
		 * Allocates a region from upload/download staging buffers and reports whether a flush/stall is required.
		 */
		Error _staging_buffer_allocate(StagingBuffers& p_staging_buffers, uint32_t p_amount, uint32_t p_required_align,
			uint32_t& r_alloc_offset, uint32_t& r_alloc_size, StagingRequiredAction& r_required_action, bool p_can_segment = true);

		/**
		 * Performs the synchronization action requested by staging allocation.
		 */
		void _staging_buffer_execute_required_action(StagingBuffers& p_staging_buffers, StagingRequiredAction p_required_action);

		/**
		 * Creates and maps a new CPU-visible staging buffer block.
		 */
		Error _insert_staging_block(StagingBuffers& p_staging_buffers);

#pragma endregion
		
#pragma region Render pass

		/**
		 * Converts a VRS method to the render-pass attachment layout it requires.
		 */
		static RDD::TextureLayout _vrs_layout_from_method(VRSMethod p_method);

		/**
		 * Validates framebuffer attachment/pass descriptions and creates a driver render pass.
		 */
		static RDD::RenderPassID _render_pass_create(RenderingDeviceDriver* p_driver, const Util::SmallVector<AttachmentFormat>& p_attachments,
			const Util::SmallVector<FramebufferPass>& p_passes, std::span<RDD::AttachmentLoadOp> p_load_ops,
			std::span<RDD::AttachmentStoreOp> p_store_ops, uint32_t p_view_count = 1, VRSMethod p_vrs_method = VRS_METHOD_NONE,
			int32_t p_vrs_attachment = -1, Size2i p_vrs_texel_size = Size2i(), Util::SmallVector<TextureSamples>* r_samples = nullptr);

#pragma endregion

#pragma region Resource cleanup

		/**
		 * Queues a RID-owned resource for deferred destruction on the current frame.
		 */
		void _free_internal(RID p_id);

		/**
		 * Waits for a frame fence when that frame has submitted GPU work.
		 */
		void _stall_for_frame(uint32_t p_frame);

		/**
		 * Waits for all frame fences before destructive or resizing operations.
		 */
		void _stall_for_previous_frames();

		/**
		 * Ends/submits current work and stalls until all frame resources are safe to reuse.
		 */
		void _flush_and_stall_for_all_frames(bool p_begin_frame = true);

		/**
		 * Tracks that one RID depends on another so dependent resources are freed first.
		 */
		void _add_dependency(RID p_id, RID p_depends_on);

		/**
		 * Recursively frees resources that depend on a RID and removes reverse dependency links.
		 */
		void _free_dependencies(RID p_id);

		/**
		 * Evicts stale cached uniform sets after a maximum frame age.
		 */
		void _uniform_set_cache_tick(uint32_t p_max_age = 8);

		/**
		 * Frees all RIDs owned by an RID owner, warning when leaked resources remain at shutdown.
		 */
		template <typename T>
		void _free_rids(T& p_owner, const char* p_type);

		/**
		 * Releases resources queued for deferred destruction once their frame is safe.
		 */
		void _free_pending_resources(int p_frame);

#pragma endregion

		RenderingDevice();
		~RenderingDevice();

	private:
		bool is_main_instance = false;
		bool pipeline_cache_enabled = false;

		std::unique_ptr<Vulkan::ImGuiDevice> imgui_device;

		RenderingContextDriver* context = nullptr;
		RenderingDeviceDriver* driver = nullptr;
		RenderingContextDriver::Device device;

		RDD::CommandQueueFamilyID main_queue_family;
		RDD::CommandQueueFamilyID transfer_queue_family;
		RDD::CommandQueueFamilyID present_queue_family;
		RDD::CommandQueueID main_queue;
		RDD::CommandQueueID transfer_queue;
		RDD::CommandQueueID present_queue;

		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID> screen_swap_chains;
		std::unordered_map<DisplayServerEnums::WindowID, RDD::FramebufferID> screen_framebuffers;

		// This is a cache and it's never freed, it ensures that
		// ID used for a specific format always remain the same.
		std::unordered_map<VertexDescriptionKey, VertexFormatID, VertexDescriptionHash> vertex_format_cache = {};
		std::unordered_map<VertexFormatID, VertexDescriptionCache> vertex_formats;

		std::map<UniformSetFormat, uint32_t> uniform_set_format_cache;

		struct UniformSetCacheKey {
			struct Binding {
				uint32_t binding = 0;
				uint32_t type = 0;
				Util::SmallVector<RID> ids;

				bool operator==(const Binding& other) const {
					return binding == other.binding &&
						type == other.type &&
						ids == other.ids;
				}
			};

			RID shader;
			uint32_t set = 0;
			Util::SmallVector<Binding> bindings;

			bool operator==(const UniformSetCacheKey& other) const {
				return shader == other.shader &&
					set == other.set &&
					bindings == other.bindings;
			}
		};

		struct UniformSetCacheKeyHash {
			size_t operator()(const UniformSetCacheKey& key) const;
		};

		struct UniformSetCacheEntry {
			RID rid;
			uint64_t last_used_frame = 0;
		};

		std::unordered_map<UniformSetCacheKey, UniformSetCacheEntry, UniformSetCacheKeyHash> uniform_set_cache;

		std::map<FramebufferFormatKey, FramebufferFormatID> framebuffer_format_cache;
		std::unordered_map<FramebufferFormatID, FramebufferFormat> framebuffer_formats;
		std::unordered_map<RID, RDD::FramebufferID> rid_to_frame_buffer_id;

		std::unordered_map<RID, std::unordered_set<RID>> dependency_map; // IDs to IDs that depend on it.
		std::unordered_map<RID, std::unordered_set<RID>> reverse_dependency_map; // Same as above, but in reverse.

		std::unique_ptr<FramebufferCache> fb_cache;
		std::unique_ptr<TransientTextureCache> tex_cache;
Util::SmallVector<TransferWorker*> transfer_worker_pool;
		uint32_t transfer_worker_pool_size = 0;
		uint32_t transfer_worker_pool_max_size = 1;
		Util::SmallVector<uint64_t> transfer_worker_operation_used_by_draw;
		Util::SmallVector<uint32_t> transfer_worker_pool_available_list;
		Util::SmallVector<RDD::TextureBarrier> transfer_worker_pool_texture_barriers;
		std::mutex transfer_worker_pool_mutex;
		std::mutex transfer_worker_pool_texture_barriers_mutex;
		std::condition_variable transfer_worker_pool_condition;

		VRSMethod vrs_method = VRS_METHOD_NONE;
		DataFormat vrs_format = DATA_FORMAT_MAX;
		Size2i vrs_texel_size;

		Util::SmallVector<Frame> frames;
		int frame = 0;
		uint64_t frames_drawn = 0;

		uint32_t frames_pending_resources_for_processing = 0u;

		std::unique_ptr<Compiler::GLSLCompiler> compiler;

		// Flag for batching descriptor sets.
		bool descriptor_set_batching = true;
		// When true, the final draw call that copies our offscreen result into the Swapchain is put into its
		// own cmd buffer, so that the whole rendering can start early instead of having to wait for the
		// swapchain semaphore to be signaled (which causes bubbles).
		bool split_swapchain_into_its_own_cmd_buffer = true;
		uint32_t gpu_copy_count = 0;
		uint32_t direct_copy_count = 0;
		uint32_t copy_bytes_count = 0;
		uint32_t prev_gpu_copy_count = 0;
		uint32_t prev_copy_bytes_count = 0;

		StagingBuffers upload_staging_buffers;
		StagingBuffers download_staging_buffers;


		uint64_t texture_memory = 0;
		uint64_t buffer_memory = 0;
		uint32_t texture_upload_region_size_px = 0;
		uint32_t texture_download_region_size_px = 0;

		Util::SmallVector<UniformSet::AttachableTexture> attachable_textures;
		Util::SmallVector<UniformSet::SharedTexture> shared_textures_to_update;

		std::unordered_map<std::string, RID> shader_name_rid_map; // shader name to rid

		std::unordered_map<uint32_t, RID> shader_cache;


		RID imgui_texture_rid;

		RID_Owner<RDD::SamplerID, true> sampler_owner;

		RID_Owner<RenderPipeline, true> render_pipeline_owner;
		RID_Owner<ComputePipeline, true> compute_pipeline_owner;

		RID_Owner<Shader, true> shader_owner;

		RID_Owner<Framebuffer, true> framebuffer_owner;

		RID_Owner<Buffer, true> vertex_buffer_owner;

		RID_Owner<IndexBuffer, true> index_buffer_owner;

		RID_Owner<VertexArray, true> vertex_array_owner;

		RID_Owner<IndexArray, true> index_array_owner;

		RID_Owner<Buffer, true> uniform_buffer_owner;
		RID_Owner<Buffer, true> storage_buffer_owner;
		RID_Owner<Buffer, true> texture_buffer_owner;

		RID_Owner<UniformSet, true> uniform_set_owner;

		RID_Owner<Texture, true> texture_owner;
	};

	template <typename T>
	void Rendering::RenderingDevice::_free_rids(T& p_owner, const char* p_type)
	{
		Util::SmallVector<RID> owned = p_owner.get_owned_list();
		if (owned.size()) {
			if (owned.size() == 1) {
				WARN_PRINT(std::format("1 RID of type '{}' was leaked.", p_type));
			}
			else {
				WARN_PRINT(std::format("%d RIDs of type '{}' were leakedM.", owned.size(), p_type));
			}
			for (const RID& rid : owned) {
#ifdef DEV_ENABLED
				if (resource_names.has(rid)) {
					print_line(String(" - ") + resource_names[rid]);
				}
#endif
				free_rid(rid);
			}
		}
	}

}
