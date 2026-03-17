#pragma once
#include "rendering/rendering_shader_container.h"
#include "rendering/rendering_device_commons.h"

namespace Vulkan
{
	using namespace ::Rendering;
	class RenderingShaderContainerVulkan : public Rendering::RenderingShaderContainer
	{

	public:
		static const uint32_t FORMAT_VERSION;

		enum CompressionFlagsVulkan {
			COMPRESSION_FLAG_SMOLV = 0x10000,
		};

		bool debug_info_enabled = false;

	protected:
		virtual uint32_t _format() const override;
		virtual uint32_t _format_version() const override;
		virtual bool _set_code_from_spirv(const ReflectShader& p_shader) override;

	public:
		RenderingShaderContainerVulkan(bool p_debug_info_enabled);
	};

	class RenderingShaderContainerFormatVulkan : public Rendering::RenderingShaderContainerFormat {
	private:
		bool debug_info_enabled = true;

	public:
		virtual ::Rendering::RenderingShaderContainer* create_container() const override;
		virtual RenderingDeviceCommons::ShaderLanguageVersion get_shader_language_version() const override;
		virtual RenderingDeviceCommons::ShaderSpirvVersion get_shader_spirv_version() const override;
		void set_debug_info_enabled(bool p_debug_info_enabled);
		bool get_debug_info_enabled() const;
		RenderingShaderContainerFormatVulkan();
		virtual ~RenderingShaderContainerFormatVulkan();
	};
}
