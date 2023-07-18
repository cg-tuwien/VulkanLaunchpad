/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 * Created by Johannes Unterguggenberger (junt@cg.tuwien.ac.at, https://johannesugb.github.io).
 */
#include "VulkanLaunchpad.h"
#include <vulkan/vulkan.hpp>
#ifdef VKL_HAS_VMA
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#endif

#include <unordered_map>
#include <map>
#include <deque>
#include <variant>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
//#define USE_SHADERC
#define USE_GLSLANG

// Always use GLI, since the manual implementation of DDS loading does not currently work.
#define USE_GLI

#ifdef USE_SHADERC
#include <shaderc/shaderc.hpp>
#endif
#ifdef USE_GLSLANG
#include <glslang/Include/glslang_c_interface.h>
#endif

#include <fstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef USE_GLI
#include <gli/load.hpp> // load DDS, KTX or KMG textures from files or memory.
#include <gli/core/flip.hpp>
#include <gli/convert.hpp> // convert a texture from a format to another
#include <gli/copy.hpp> // copy a texture or subset of a texture to another texture
#include <gli/duplicate.hpp> // duplicate the data of a texture, allocating a new texture storage
#include <gli/format.hpp> // list of the supported formats
#include <gli/generate_mipmaps.hpp> // generating the mipmaps of a texture
#include <gli/make_texture.hpp> // helper functions to create generic texture
#include <gli/texture2d.hpp>
#endif

vk::Instance mInstance                   = {};
vk::SurfaceKHR mSurface                  = {};
vk::PhysicalDevice mPhysicalDevice       = {};
vk::Device mDevice                       = {};
vk::DispatchLoaderStatic mDispatchLoader = {};
vk::Queue mQueue                         = {};
VklSwapchainConfig mSwapchainConfig      = {};
std::vector<std::vector<vk::ClearValue>> mClearValues;

#ifdef VKL_HAS_VMA
VmaAllocator mVmaAllocator               = {};
bool vklHasVmaAllocator()                { return VmaAllocator{} != mVmaAllocator; }
#endif

bool mFrameworkInitialized = false;

vk::DispatchLoaderDynamic mDynamicDispatch;
vk::ResultValueType<VULKAN_HPP_NAMESPACE::DebugUtilsMessengerEXT>::type mDebugUtilsMessenger;
std::vector<std::vector<vk::ImageView>> mSwapchainImageViews; //< Will be the length of #swapchain images
vk::PipelineStageFlags mSrcStages0;
vk::AccessFlags mSrcAccess0;
vk::PipelineStageFlags mDstStages0;
vk::AccessFlags mDstAccess0;
vk::UniqueRenderPass mRenderpass;
std::vector<vk::UniqueFramebuffer> mFramebuffers; //< Will be the length of #swapchain images
bool mHasDepthAttachments = false;

constexpr int CONCURRENT_FRAMES = 1;
std::array<vk::UniqueSemaphore, CONCURRENT_FRAMES> mImageAvailableSemaphores;
std::array<vk::UniqueSemaphore, CONCURRENT_FRAMES> mRenderFinishedSemaphores;
std::array<vk::UniqueFence, CONCURRENT_FRAMES> mSyncHostWithDeviceFence;
std::vector<int> mImagesInFlightFenceIndices;

int64_t mFrameId;
int mFrameInFlightIndex;
uint32_t mCurrentSwapChainImageIndex;

vk::UniqueCommandPool mCommandPool;
#ifdef VKL_HAS_VMA
std::unordered_map<VkBuffer, std::variant<vk::UniqueDeviceMemory, VmaAllocation>> mHostCoherentBuffersWithBackingMemory;
std::unordered_map<VkBuffer, std::variant<vk::UniqueDeviceMemory, VmaAllocation>> mDeviceLocalBuffersWithBackingMemory;
std::unordered_map<VkImage, std::variant<vk::UniqueDeviceMemory, VmaAllocation>> mImagesWithBackingMemory;
#else
std::unordered_map<VkBuffer, vk::UniqueDeviceMemory> mHostCoherentBuffersWithBackingMemory;
std::unordered_map<VkBuffer, vk::UniqueDeviceMemory> mDeviceLocalBuffersWithBackingMemory;
std::unordered_map<VkImage, vk::UniqueDeviceMemory> mImagesWithBackingMemory;
#endif
std::deque<vk::UniqueCommandBuffer> mSingleUseCommandBuffers;

std::unordered_map<VkPipeline, std::tuple<vk::UniqueDescriptorSetLayout, vk::UniquePipelineLayout>> mPipelineLayouts;

vk::Pipeline mBasicPipeline;

// TODO: Implement this MAKEFOURCC in a sane way instead of just copying definitions.
enum class byte : unsigned char {};
#ifndef _BYTE_DEFINED
#define _BYTE_DEFINED
typedef byte BYTE;
#endif // !_BYTE_DEFINED
#ifndef _DWORD_DEFINED
#define _DWORD_DEFINED
typedef unsigned long DWORD;
#endif // !_DWORD_DEFINED

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define FOURCC_DXT1	MAKEFOURCC('D', 'X', 'T', '1')
#define FOURCC_DXT3	MAKEFOURCC('D', 'X', 'T', '3')
#define FOURCC_DXT5	MAKEFOURCC('D', 'X', 'T', '5')

// Debug utils messenger callback:
VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data);

std::string mSpaceForToString;
const char* to_string(VkResult result)
{
	switch (result) {
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_EVENT_SET: return "VK_EVENT_SET";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
	case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
	case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
	case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
	case VK_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
	default:
		mSpaceForToString = std::to_string(result);
		return mSpaceForToString.c_str();
	}
}

#ifdef USE_SHADERC
std::string to_string(shaderc_shader_kind kind)
{
	switch (kind) {
	case shaderc_vertex_shader: return "shaderc_vertex_shader";
	case shaderc_fragment_shader: return "shaderc_fragment_shader";
	case shaderc_compute_shader: return "shaderc_compute_shader";
	case shaderc_geometry_shader: return "shaderc_geometry_shader";
	case shaderc_tess_control_shader: return "shaderc_tess_control_shader";
	case shaderc_tess_evaluation_shader: return "shaderc_tess_evaluation_shader";
	case shaderc_glsl_infer_from_source: return "shaderc_glsl_infer_from_source";
	case shaderc_glsl_default_vertex_shader: return "shaderc_glsl_default_vertex_shader";
	case shaderc_glsl_default_fragment_shader: return "shaderc_glsl_default_fragment_shader";
	case shaderc_glsl_default_compute_shader: return "shaderc_glsl_default_compute_shader";
	case shaderc_glsl_default_geometry_shader: return "shaderc_glsl_default_geometry_shader";
	case shaderc_glsl_default_tess_control_shader: return "shaderc_glsl_default_tess_control_shader";
	case shaderc_glsl_default_tess_evaluation_shader: return "shaderc_glsl_default_tess_evaluation_shader";
	case shaderc_spirv_assembly: return "shaderc_spirv_assembly";
	case shaderc_raygen_shader: return "shaderc_raygen_shader";
	case shaderc_anyhit_shader: return "shaderc_anyhit_shader";
	case shaderc_closesthit_shader: return "shaderc_closesthit_shader";
	case shaderc_miss_shader: return "shaderc_miss_shader";
	case shaderc_intersection_shader: return "shaderc_intersection_shader";
	case shaderc_callable_shader: return "shaderc_callable_shader";
	case shaderc_glsl_default_raygen_shader: return "shaderc_glsl_default_raygen_shader";
	case shaderc_glsl_default_anyhit_shader: return "shaderc_glsl_default_anyhit_shader";
	case shaderc_glsl_default_closesthit_shader: return "shaderc_glsl_default_closesthit_shader";
	case shaderc_glsl_default_miss_shader: return "shaderc_glsl_default_miss_shader";
	case shaderc_glsl_default_intersection_shader: return "shaderc_glsl_default_intersection_shader";
	case shaderc_glsl_default_callable_shader: return "shaderc_glsl_default_callable_shader";
	case shaderc_task_shader: return "shaderc_task_shader";
	case shaderc_mesh_shader: return "shaderc_mesh_shader";
	case shaderc_glsl_default_task_shader: return "shaderc_glsl_default_task_shader";
	case shaderc_glsl_default_mesh_shader: return "shaderc_glsl_default_mesh_shader";
	default: return std::to_string(kind);
	}
}
#endif
#ifdef USE_GLSLANG
std::string to_string(glslang_stage_t stage)
{
	switch (stage) {
	case GLSLANG_STAGE_VERTEX: return "GLSLANG_STAGE_VERTEX";
	case GLSLANG_STAGE_TESSCONTROL: return "GLSLANG_STAGE_TESSCONTROL";
	case GLSLANG_STAGE_TESSEVALUATION: return "GLSLANG_STAGE_TESSEVALUATION";
	case GLSLANG_STAGE_GEOMETRY: return "GLSLANG_STAGE_GEOMETRY";
	case GLSLANG_STAGE_FRAGMENT: return "GLSLANG_STAGE_FRAGMENT";
	case GLSLANG_STAGE_COMPUTE: return "GLSLANG_STAGE_COMPUTE";
	case GLSLANG_STAGE_RAYGEN_NV: return "GLSLANG_STAGE_RAYGEN_NV";
	case GLSLANG_STAGE_INTERSECT_NV: return "GLSLANG_STAGE_INTERSECT_NV";
	case GLSLANG_STAGE_ANYHIT_NV: return "GLSLANG_STAGE_ANYHIT_NV";
	case GLSLANG_STAGE_CLOSESTHIT_NV: return "GLSLANG_STAGE_CLOSESTHIT_NV";
	case GLSLANG_STAGE_MISS_NV: return "GLSLANG_STAGE_MISS_NV";
	case GLSLANG_STAGE_CALLABLE_NV: return "GLSLANG_STAGE_CALLABLE_NV";
	case GLSLANG_STAGE_TASK_NV: return "GLSLANG_STAGE_TASK_NV";
	case GLSLANG_STAGE_MESH_NV: return "GLSLANG_STAGE_MESH_NV";
	default: return std::to_string(stage);
	}
}

glslang_resource_t get_default_resource() {
	glslang_resource_t r = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxMeshOutputVerticesEXT = */ 256,
		/* .maxMeshOutputPrimitivesEXT = */ 256,
		/* .maxMeshWorkGroupSizeX_EXT = */ 128,
		/* .maxMeshWorkGroupSizeY_EXT = */ 128,
		/* .maxMeshWorkGroupSizeZ_EXT = */ 128,
		/* .maxTaskWorkGroupSizeX_EXT = */ 128,
		/* .maxTaskWorkGroupSizeY_EXT = */ 128,
		/* .maxTaskWorkGroupSizeZ_EXT = */ 128,
		/* .maxMeshViewCountEXT = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,

		/* .limits = */ {
			/* .nonInductiveForLoops = */ 1,
			/* .whileLoops = */ 1,
			/* .doWhileLoops = */ 1,
			/* .generalUniformIndexing = */ 1,
			/* .generalAttributeMatrixVectorIndexing = */ 1,
			/* .generalVaryingIndexing = */ 1,
			/* .generalSamplerIndexing = */ 1,
			/* .generalVariableIndexing = */ 1,
			/* .generalConstantMatrixVectorIndexing = */ 1,
		}
	};
	return r;
}
#endif

// Compiles a shader to a SPIR-V binary. Returns the binary as a vector of 32-bit words.
std::vector<uint32_t> compileShaderSourceToSpirv(const std::string& shaderSource, const std::string& inputFilename
#ifdef USE_SHADERC
	, shaderc_shader_kind shaderKind
#endif
#ifdef USE_GLSLANG
	, glslang_stage_t shaderStage
#endif
)
{
#ifdef USE_SHADERC
	// This code is borrowed from the shaderc example: https://github.com/google/shaderc/blob/main/examples/online-compile/main.cc
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shaderSource, shaderKind, inputFilename.c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cout << "\nERROR:   Failed to compile shader[" << inputFilename << " of kind[" << to_string(shaderKind) << "]\n"
			      << "\n         Reason(s)[\n" << module.GetErrorMessage() << "\n         ]" << std::endl;

		throw std::runtime_error("Failed to compile shader " + inputFilename);
	}

	return { module.cbegin(), module.cend() };
#else
	std::vector<uint32_t> resultingSpirv;
#ifdef USE_GLSLANG
	const char* shaderCode = shaderSource.c_str();

	static const auto defaultResources = get_default_resource();

	glslang_input_t input = {};
	input.language = GLSLANG_SOURCE_GLSL;
	input.stage = shaderStage;
	input.client = GLSLANG_CLIENT_VULKAN;
	// Looks like Vulkan 1.1 is fine even though we're linking against a Vulkan 1.2 SDK:
	input.client_version = GLSLANG_TARGET_VULKAN_1_1;
	input.target_language = GLSLANG_TARGET_SPV;
	// SPIR-V 1.5 has been released on September 13th, 2019 to accompany the launch of Vulkan 1.2
	// However, Vulkan 1.1 requires Spir-V 1.3, go with 1.3 to match the Vulkan 1.1 target above:
	input.target_language_version = GLSLANG_TARGET_SPV_1_3; 
	input.code = shaderCode;
	input.default_version = 100;
	input.default_profile = GLSLANG_NO_PROFILE;
	input.force_default_version_and_profile = false;
	input.forward_compatible = false;
	input.messages = GLSLANG_MSG_DEFAULT_BIT;
	input.resource = &defaultResources;
	
	glslang_shader_t* shader = glslang_shader_create(&input);

	if (!glslang_shader_preprocess(shader, &input))
	{
		std::cout << "\nERROR:   Failed to preprocess shader[" << inputFilename << "] of kind[" << to_string(shaderStage) << "]"
			      << "\n         Log[" << glslang_shader_get_info_log(shader) << "]"
			      << "\n         Debug-Log[" << glslang_shader_get_info_debug_log(shader) << "]" << std::endl;

		VKL_EXIT_WITH_ERROR("glslang_shader_preprocess failed for " + inputFilename);
	}

	if (!glslang_shader_parse(shader, &input))
	{
		std::cout << "\nERROR:   Failed to parse shader[" << inputFilename << "] of kind[" << to_string(shaderStage) << "]"
			      << "\n         Log[" << glslang_shader_get_info_log(shader) << "]"
			      << "\n         Debug-Log[" << glslang_shader_get_info_debug_log(shader) << "]" << std::endl;

		VKL_EXIT_WITH_ERROR("glslang_shader_parse failed for " + inputFilename);
	}

	glslang_program_t* program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
	{
		std::cout << "\nERROR:   Failed to link shader[" << inputFilename << "] of kind[" << to_string(shaderStage) << "]"
			      << "\n         Log[" << glslang_shader_get_info_log(shader) << "]"
			      << "\n         Debug-Log[" << glslang_shader_get_info_debug_log(shader) << "]" << std::endl;

		VKL_EXIT_WITH_ERROR("glslang_program_link failed for " + inputFilename);
	}

	glslang_program_SPIRV_generate(program, input.stage);

	if (glslang_program_SPIRV_get_messages(program))
	{
		printf("%s", glslang_program_SPIRV_get_messages(program));
		std::cout << "\nINFO:    Got messages for shader[" << inputFilename << " of kind[" << to_string(shaderStage) << "]"
			      << "\n         Message[" << glslang_program_SPIRV_get_messages(program) << "]" << std::endl;
	}

	auto* spirvDataPtr = glslang_program_SPIRV_get_ptr(program);
	const auto spirvNumWords = glslang_program_SPIRV_get_size(program);
	resultingSpirv.insert(std::end(resultingSpirv), spirvDataPtr, spirvDataPtr + spirvNumWords);

	glslang_program_delete(program);
	glslang_shader_delete(shader);
	
#endif
	return resultingSpirv;
#endif
}

// Creates a shader module from the given Spir-V code, returns the created shader module and its create info.
// The entry point is "main" always
std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> loadShaderFromSpirvAndCreateShaderModuleAndStageInfo(const uint32_t* spirv, size_t byteSize, const vk::ShaderStageFlagBits shaderStage)
{
	auto moduleCreateInfo = vk::ShaderModuleCreateInfo{}
		.setCodeSize(byteSize)
		.setPCode(spirv);

	auto shaderModule = mDevice.createShaderModule(moduleCreateInfo);

	auto shaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{}
		.setStage(shaderStage)
		.setModule(shaderModule)
		.setPName("main"); // entry point

	return std::make_tuple(shaderModule, shaderStageCreateInfo);
}

std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> loadShaderFromMemoryAndCreateShaderModuleAndStageInfo(const std::string& shaderCode, const std::string& shaderName, const vk::ShaderStageFlagBits shaderStage)
{
#ifdef USE_SHADERC
	shaderc_shader_kind shadercKind;
	switch (shaderStage) {
	case vk::ShaderStageFlagBits::eVertex: shadercKind = shaderc_shader_kind::shaderc_vertex_shader; break;
	case vk::ShaderStageFlagBits::eTessellationControl: shadercKind = shaderc_shader_kind::shaderc_tess_control_shader; break;
	case vk::ShaderStageFlagBits::eTessellationEvaluation: shadercKind = shaderc_shader_kind::shaderc_tess_evaluation_shader; break;
	case vk::ShaderStageFlagBits::eGeometry: shadercKind = shaderc_shader_kind::shaderc_geometry_shader; break;
	case vk::ShaderStageFlagBits::eFragment: shadercKind = shaderc_shader_kind::shaderc_fragment_shader; break;
	case vk::ShaderStageFlagBits::eCompute: shadercKind = shaderc_shader_kind::shaderc_compute_shader; break;
	case vk::ShaderStageFlagBits::eRaygenKHR: shadercKind = shaderc_shader_kind::shaderc_raygen_shader; break;
	case vk::ShaderStageFlagBits::eAnyHitKHR: shadercKind = shaderc_shader_kind::shaderc_anyhit_shader; break;
	case vk::ShaderStageFlagBits::eClosestHitKHR: shadercKind = shaderc_shader_kind::shaderc_closesthit_shader; break;
	case vk::ShaderStageFlagBits::eMissKHR: shadercKind = shaderc_shader_kind::shaderc_miss_shader; break;
	case vk::ShaderStageFlagBits::eIntersectionKHR: shadercKind = shaderc_shader_kind::shaderc_intersection_shader; break;
	case vk::ShaderStageFlagBits::eCallableKHR: shadercKind = shaderc_shader_kind::shaderc_callable_shader; break;
	case vk::ShaderStageFlagBits::eTaskNV: shadercKind = shaderc_shader_kind::shaderc_task_shader; break;
	case vk::ShaderStageFlagBits::eMeshNV: shadercKind = shaderc_shader_kind::shaderc_mesh_shader; break;
	}
	auto spirv = compileShaderSourceToSpirv(shaderCode, shaderName, shadercKind);
#endif
#ifdef USE_GLSLANG
	glslang_stage_t glslangStage;
	switch (shaderStage) {
	case vk::ShaderStageFlagBits::eVertex: glslangStage = GLSLANG_STAGE_VERTEX; break;
	case vk::ShaderStageFlagBits::eTessellationControl: glslangStage = GLSLANG_STAGE_TESSCONTROL; break;
	case vk::ShaderStageFlagBits::eTessellationEvaluation: glslangStage = GLSLANG_STAGE_TESSEVALUATION; break;
	case vk::ShaderStageFlagBits::eGeometry: glslangStage = GLSLANG_STAGE_GEOMETRY; break;
	case vk::ShaderStageFlagBits::eFragment: glslangStage = GLSLANG_STAGE_FRAGMENT; break;
	case vk::ShaderStageFlagBits::eCompute: glslangStage = GLSLANG_STAGE_COMPUTE; break;
	case vk::ShaderStageFlagBits::eRaygenKHR: glslangStage = GLSLANG_STAGE_RAYGEN; break;
	case vk::ShaderStageFlagBits::eAnyHitKHR: glslangStage = GLSLANG_STAGE_ANYHIT; break;
	case vk::ShaderStageFlagBits::eClosestHitKHR: glslangStage = GLSLANG_STAGE_CLOSESTHIT; break;
	case vk::ShaderStageFlagBits::eMissKHR: glslangStage = GLSLANG_STAGE_MISS; break;
	case vk::ShaderStageFlagBits::eIntersectionKHR: glslangStage = GLSLANG_STAGE_INTERSECT; break;
	case vk::ShaderStageFlagBits::eCallableKHR: glslangStage = GLSLANG_STAGE_CALLABLE; break;
	case vk::ShaderStageFlagBits::eTaskNV: glslangStage = GLSLANG_STAGE_TASK; break;
	case vk::ShaderStageFlagBits::eMeshNV: glslangStage = GLSLANG_STAGE_MESH; break;
	}
	auto spirv = compileShaderSourceToSpirv(shaderCode, shaderName, glslangStage);
#endif
	//                                                        | SPIR-V Code | Size must be specified in BYTE => * sizeof WORD   | Stage      |
	return loadShaderFromSpirvAndCreateShaderModuleAndStageInfo(spirv.data(), spirv.size() * sizeof(decltype(spirv)::value_type), shaderStage);
}

std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> loadShaderFromFileAndCreateShaderModuleAndStageInfo(const std::string& shader_filename, const vk::ShaderStageFlagBits shaderStage)
{
    std::string path = {};

	std::ifstream infile(shader_filename);
	if (infile.good()) {
		path = shader_filename;
		VKL_LOG("Loading shader file from path[" << path << "]...");
	}

	if (path.empty()) { // Fail if shader file could not be found:
		VKL_EXIT_WITH_ERROR("Unable to load file[" << shader_filename << "].");
	}

	std::ifstream ifs(path);
	std::string content(
		(std::istreambuf_iterator<char>(ifs)),
		(std::istreambuf_iterator<char>())
	);

	return loadShaderFromMemoryAndCreateShaderModuleAndStageInfo(content, path, shaderStage);
}

VkPipeline vklCreateGraphicsPipeline(const VklGraphicsPipelineConfig& config, bool loadShadersFromMemoryInstead)
{
    if (!loadShadersFromMemoryInstead && !vklFrameworkInitialized()) {
        VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
    }
    // Create the graphics pipeline, describe every state of it
	// Get tuples of <vk::ShaderModule, vk::PipelineShaderStageCreateInfo>
	auto vertTpl = loadShadersFromMemoryInstead
		? loadShaderFromMemoryAndCreateShaderModuleAndStageInfo(config.vertexShaderPath, "vertex shader from memory", vk::ShaderStageFlagBits::eVertex)
		: loadShaderFromFileAndCreateShaderModuleAndStageInfo(config.vertexShaderPath, vk::ShaderStageFlagBits::eVertex);

	auto fragTpl = loadShadersFromMemoryInstead
		? loadShaderFromMemoryAndCreateShaderModuleAndStageInfo(config.fragmentShaderPath, "fragment shader from memory", vk::ShaderStageFlagBits::eFragment)
		: loadShaderFromFileAndCreateShaderModuleAndStageInfo(config.fragmentShaderPath, vk::ShaderStageFlagBits::eFragment);

	// Describe the shaders used:
	std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{ std::get<vk::PipelineShaderStageCreateInfo>(vertTpl), std::get<vk::PipelineShaderStageCreateInfo>(fragTpl) };
	// Describe the vertex input, i.e. two vertex input attributes in our case:

	std::vector<vk::VertexInputBindingDescription> inputBufferBindings(std::begin(config.vertexInputBuffers), std::end(config.vertexInputBuffers));
	std::vector<vk::VertexInputAttributeDescription> inputAttributeDescriptions(std::begin(config.inputAttributeDescriptions), std::end(config.inputAttributeDescriptions));

	auto vertexInputState = vk::PipelineVertexInputStateCreateInfo{}
		.setVertexBindingDescriptionCount(static_cast<uint32_t>(inputBufferBindings.size())).setPVertexBindingDescriptions(inputBufferBindings.data())
		.setVertexAttributeDescriptionCount(static_cast<uint32_t>(inputAttributeDescriptions.size())).setPVertexAttributeDescriptions(inputAttributeDescriptions.data());
	// Describe the topology of the vertices
	auto inputAssemblyState = vk::PipelineInputAssemblyStateCreateInfo{}.setTopology(vk::PrimitiveTopology::eTriangleList);
	// Describe viewport and scissors state
	auto viewport = vk::Viewport{}
		.setX(0.0f).setY(0.0f)
		.setWidth(static_cast<float>(mSwapchainConfig.imageExtent.width)).setHeight(static_cast<float>(mSwapchainConfig.imageExtent.height))
		.setMinDepth(0.0f).setMaxDepth(1.0f);
	auto scissors = vk::Rect2D{}.setOffset({ 0, 0 }).setExtent(mSwapchainConfig.imageExtent);
	auto viewportState = vk::PipelineViewportStateCreateInfo{}
		.setViewportCount(1u).setPViewports(&viewport)
		.setScissorCount(1u).setPScissors(&scissors);
	// Describe the rasterizer state
	auto rasterizerState = vk::PipelineRasterizationStateCreateInfo{}
		.setPolygonMode(static_cast<vk::PolygonMode>(config.polygonDrawMode))
		.setLineWidth(1.0f) // reasons...
		.setCullMode(static_cast<vk::CullModeFlags>(config.triangleCullingMode))
		.setFrontFace(vk::FrontFace::eCounterClockwise);
	// Describe multisampling state
	auto multisampleState = vk::PipelineMultisampleStateCreateInfo{}.setRasterizationSamples(vk::SampleCountFlagBits::e1);
	// Configure depth/stencil state
	auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{}
		.setDepthTestEnable( mHasDepthAttachments ? VK_TRUE : VK_FALSE)
		.setDepthWriteEnable(mHasDepthAttachments ? VK_TRUE : VK_FALSE)
		.setDepthCompareOp(vk::CompareOp::eLess);
	// Configure blending and which color channels are written
	auto colorBlendAttachmentState = vk::PipelineColorBlendAttachmentState{}
		.setBlendEnable(VK_FALSE)
		.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA); // write all color components

	if (config.enableAlphaBlending) {
		colorBlendAttachmentState
			.setBlendEnable(VK_TRUE)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setAlphaBlendOp(vk::BlendOp::eAdd);
	}

	auto colorBlendState = vk::PipelineColorBlendStateCreateInfo{}.setAttachmentCount(1u).setPAttachments(&colorBlendAttachmentState);
	
	// But again: not so fast! We have to define the LAYOUT of our descriptors first
	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings(std::begin(config.descriptorLayout), std::end(config.descriptorLayout));
	auto descriptorSetLayout = mDevice.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo{}
			.setBindingCount(static_cast<uint32_t>(layoutBindings.size()))
			.setPBindings(layoutBindings.data())
		, nullptr, mDispatchLoader
	);

	// Continue with configuring our graphics pipeline:
	// Create a PIPELINE LAYOUT which describes all RESOURCES that are passed in to our pipeline (Resource Descriptors that we have created above)
	auto pipelineLayout = mDevice.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo{} // A pipeline's layout describes all resources used by a pipeline or in shaders.
			.setSetLayoutCount(1u)
			.setPSetLayouts(&descriptorSetLayout.get()) // We don't need the actual descriptors when defining the PIPELINE. The LAYOUT is sufficient at this point.
		, nullptr, mDispatchLoader
	);

	// Put everything together:
	auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo{}
		.setStageCount(static_cast<uint32_t>(shaderStages.size())).setPStages(shaderStages.data())
		.setPVertexInputState(&vertexInputState)
		.setPInputAssemblyState(&inputAssemblyState)
		.setPViewportState(&viewportState)
		.setPRasterizationState(&rasterizerState)
		.setPMultisampleState(&multisampleState)
		.setPDepthStencilState(&depthStencilState)
		.setPColorBlendState(&colorBlendState)
		.setLayout(pipelineLayout.get())
		.setRenderPass(mRenderpass.get()).setSubpass(0u); // <--- Which subpass of the given renderpass we are going to use this graphics pipeline for
	// FINALLY:
	auto graphicsPipeline = mDevice.createGraphicsPipeline(nullptr, pipelineCreateInfo).value;
	
	// Don't need the modules anymore:
	mDevice.destroyShaderModule(std::get<vk::ShaderModule>(fragTpl));
	mDevice.destroyShaderModule(std::get<vk::ShaderModule>(vertTpl));

	mPipelineLayouts[static_cast<VkPipeline>(graphicsPipeline)] = std::forward_as_tuple(std::move(descriptorSetLayout), std::move(pipelineLayout));
	return static_cast<VkPipeline>(graphicsPipeline);
}

void vklDestroyGraphicsPipeline(VkPipeline pipeline)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	mDevice.destroy(vk::Pipeline{ pipeline });
}

vk::MemoryAllocateInfo vklCreateMemoryAllocateInfo(vk::DeviceSize bufferSize, vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags) {
	auto memoryAllocInfo = vk::MemoryAllocateInfo{}
		.setAllocationSize(std::max(bufferSize, memoryRequirements.size))
		.setMemoryTypeIndex([&]() {
			// Get memory types supported by the physical device:
			auto memoryProperties = mPhysicalDevice.getMemoryProperties();

			// In search for a suitable memory type INDEX:
			int selectedMemIndex = -1;
			vk::DeviceSize selectedHeapSize = 0;
			for (int i = 0; i < static_cast<int>(memoryProperties.memoryTypeCount); ++i) {

				// Is this kind of memory suitable for our buffer?
				const auto bitmask = memoryRequirements.memoryTypeBits;
				const auto bit = 1 << i;
				if (0 == (bitmask & bit)) {
					continue; // => nope
				}

				// Does this kind of memory support our usage requirements?
				if ((memoryProperties.memoryTypes[i].propertyFlags & (memoryPropertyFlags)) != vk::MemoryPropertyFlags{}) {
					// Would support => now select the one with the largest heap:
					const auto heapSize = memoryProperties.memoryHeaps[memoryProperties.memoryTypes[i].heapIndex].size;
					if (heapSize > selectedHeapSize) {
						// We have a new king:
						selectedMemIndex = i;
						selectedHeapSize = heapSize;
					}
				}
			}

			if (-1 == selectedMemIndex) {
				VKL_EXIT_WITH_ERROR(std::string("ERROR: Couldn't find suitable memory of size[") + std::to_string(bufferSize) + "] and requirements[" + std::to_string(memoryRequirements.alignment) + ", " + std::to_string(memoryRequirements.memoryTypeBits) + ", " + std::to_string(memoryRequirements.size) + "]");
			}

			// all good, we found a suitable memory index:
			return static_cast<uint32_t>(selectedMemIndex);
		}());
	return memoryAllocInfo;
}

VkMemoryAllocateInfo vklCreateMemoryAllocateInfo(VkDeviceSize bufferSize, VkMemoryRequirements memoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags) {
  return static_cast<VkMemoryAllocateInfo>(vklCreateMemoryAllocateInfo(static_cast<vk::DeviceSize>(bufferSize), static_cast<vk::MemoryRequirements>(memoryRequirements), static_cast<vk::MemoryPropertyFlags>(memoryPropertyFlags)));
}

VkDeviceMemory vklAllocateMemoryForGivenRequirements(VkDeviceSize bufferSize, VkMemoryRequirements memoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags)
{
    const auto memoryAllocInfo = vklCreateMemoryAllocateInfo(bufferSize, memoryRequirements, memoryPropertyFlags);

	// Allocate:
	VkDeviceMemory memory;
    VkResult returnCode = vkAllocateMemory(static_cast<VkDevice>(mDevice), &memoryAllocInfo, NULL, &memory);
	if (returnCode == VK_SUCCESS) {
		return memory;
	}
	else {
    	VKL_EXIT_WITH_ERROR(std::string("Error allocating memory of size [") + std::to_string(bufferSize) + "] and requirements[" + std::to_string(memoryRequirements.alignment) + ", " + std::to_string(memoryRequirements.memoryTypeBits) + ", " + std::to_string(memoryRequirements.size) + "]\n    Error Code: " + to_string(returnCode));
	}
}

vk::UniqueDeviceMemory vklAllocateMemoryForGivenRequirements(vk::DeviceSize bufferSize, vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags) {
  const auto memoryAllocInfo = vklCreateMemoryAllocateInfo(bufferSize, memoryRequirements, memoryPropertyFlags);
  auto allocatedMemory = mDevice.allocateMemoryUnique(memoryAllocInfo, nullptr, mDispatchLoader);
  return allocatedMemory;
}

VkBuffer vklCreateHostCoherentBufferWithBackingMemory(VkDeviceSize buffer_size, VkBufferUsageFlags buffer_usage)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Describe a new buffer:
	auto createInfo = vk::BufferCreateInfo{}
		.setSize(static_cast<vk::DeviceSize>(buffer_size))
		.setUsage(vk::BufferUsageFlags{ buffer_usage });

#ifdef VKL_HAS_VMA
	if (vklHasVmaAllocator()) {
		VmaAllocationCreateInfo vmaBufferCreateInfo = {};
		vmaBufferCreateInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
		vmaBufferCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VkBuffer bufferFromVma;
		VmaAllocation vmaAllocation;
		vmaCreateBuffer(mVmaAllocator, &static_cast<const VkBufferCreateInfo&>(createInfo), &vmaBufferCreateInfo, &bufferFromVma, &vmaAllocation, nullptr);
		mHostCoherentBuffersWithBackingMemory[bufferFromVma] = std::move(vmaAllocation);
		return bufferFromVma;
	}
#endif

	auto buffer = mDevice.createBuffer(createInfo);

	// Allocate the memory (we want host-coherent memory):
    auto memory = vklAllocateMemoryForGivenRequirements(static_cast<vk::DeviceSize>(buffer_size), mDevice.getBufferMemoryRequirements(buffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
	// Bind the buffer handle to the memory:
	// mDevice.bindBufferMemory(buffer, memory.get(), 0);
	mDevice.bindBufferMemory(buffer, memory.get(), 0);

	// Remember the assignment:
	mHostCoherentBuffersWithBackingMemory[static_cast<VkBuffer>(buffer)] = std::move(memory);

	return static_cast<VkBuffer>(buffer);
}

VkBuffer vklCreateDeviceLocalBufferWithBackingMemory(VkDeviceSize buffer_size, VkBufferUsageFlags buffer_usage)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Describe a new buffer:
	auto createInfo = vk::BufferCreateInfo{}
		.setSize(static_cast<vk::DeviceSize>(buffer_size))
		.setUsage(vk::BufferUsageFlags{ buffer_usage });

#ifdef VKL_HAS_VMA
	if (vklHasVmaAllocator()) {
		VmaAllocationCreateInfo vmaBufferCreateInfo = {};
		vmaBufferCreateInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
		vmaBufferCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkBuffer bufferFromVma;
		VmaAllocation vmaAllocation;
		vmaCreateBuffer(mVmaAllocator, &static_cast<const VkBufferCreateInfo&>(createInfo), &vmaBufferCreateInfo, &bufferFromVma, &vmaAllocation, nullptr);
		mDeviceLocalBuffersWithBackingMemory[bufferFromVma] = std::move(vmaAllocation);
		return bufferFromVma;
	}
#endif

	auto buffer = mDevice.createBuffer(createInfo);

	// Allocate the memory (we want device-local memory):
	auto memory = vklAllocateMemoryForGivenRequirements(static_cast<vk::DeviceSize>(buffer_size), mDevice.getBufferMemoryRequirements(buffer), vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Bind the buffer handle to the memory:
	// mDevice.bindBufferMemory(buffer, memory.get(), 0);
	mDevice.bindBufferMemory(buffer, memory.get(), 0);

	// Remember the assignment:
	mDeviceLocalBuffersWithBackingMemory[static_cast<VkBuffer>(buffer)] = std::move(memory);

	return static_cast<VkBuffer>(buffer);
}

void vklDestroyHostCoherentBufferAndItsBackingMemory(VkBuffer buffer)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	if (VkBuffer{} == buffer) {
		VKL_EXIT_WITH_ERROR("Invalid buffer handle passed to vklDestroyHostCoherentBufferAndItsBackingMemory(...)");
	}

	bool resourceDestroyed = false;
	auto search = mHostCoherentBuffersWithBackingMemory.find(buffer);
	if (mHostCoherentBuffersWithBackingMemory.end() != search) {
#ifdef VKL_HAS_VMA
		if (vklHasVmaAllocator() && std::holds_alternative<VmaAllocation>(search->second)) {
			vmaDestroyBuffer(mVmaAllocator, buffer, std::get<VmaAllocation>(search->second));
			resourceDestroyed = true;
		}
#endif
		mHostCoherentBuffersWithBackingMemory.erase(search);
	}
	else {
		VKL_WARNING("VkDeviceMemory for the given VkBuffer not found. Are you sure that you have created this buffer with vklCreateHostCoherentBufferWithBackingMemory(...)? Are you sure that you haven't already destroyed this VkBuffer?");
	}

	if (!resourceDestroyed) {
		mDevice.destroy(vk::Buffer{ buffer });
	}
}

void vklDestroyDeviceLocalBufferAndItsBackingMemory(VkBuffer buffer)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	if (VkBuffer{} == buffer) {
		VKL_EXIT_WITH_ERROR("Invalid buffer handle passed to vklDestroyDeviceLocalBufferAndItsBackingMemory(...)");
	}

	bool resourceDestroyed = false;
	auto search = mDeviceLocalBuffersWithBackingMemory.find(buffer);
	if (mDeviceLocalBuffersWithBackingMemory.end() != search) {
#ifdef VKL_HAS_VMA
		if (vklHasVmaAllocator() && std::holds_alternative<VmaAllocation>(search->second)) {
			vmaDestroyBuffer(mVmaAllocator, buffer, std::get<VmaAllocation>(search->second));
			resourceDestroyed = true;
		}
#endif
		mDeviceLocalBuffersWithBackingMemory.erase(search);
	}
	else {
		VKL_WARNING("VkDeviceMemory for the given VkBuffer not found. Are you sure that you have created this buffer with vklCreateDeviceLocalBufferWithBackingMemory(...)? Are you sure that you haven't already destroyed this VkBuffer?");
	}

	if (!resourceDestroyed) {
		mDevice.destroy(vk::Buffer{ buffer });
	}
}

void vklCopyDataIntoHostCoherentBuffer(VkBuffer buffer, const void* data_pointer, size_t data_size_in_bytes)
{
	vklCopyDataIntoHostCoherentBuffer(buffer, 0, data_pointer, data_size_in_bytes);
}

void vklCopyDataIntoHostCoherentBuffer(VkBuffer buffer, size_t buffer_offset_in_bytes, const void* data_pointer, size_t data_size_in_bytes)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	if (VkBuffer{} == buffer) {
		VKL_EXIT_WITH_ERROR("Invalid buffer handle passed to vklCopyDataIntoHostCoherentBuffer(...)");
	}

	auto search = mHostCoherentBuffersWithBackingMemory.find(buffer);
	if (mHostCoherentBuffersWithBackingMemory.end() == search) {
		VKL_EXIT_WITH_ERROR("Couldn't find backing memory for the given VkBuffer => Can't copy data. Have you created the buffer via vklCreateHostCoherentBufferWithBackingMemory(...)?");
	}

#ifdef VKL_HAS_VMA
	if (vklHasVmaAllocator() && std::holds_alternative<VmaAllocation>(search->second)) {
		void* mappedData;
		auto result = vmaMapMemory(mVmaAllocator, std::get<VmaAllocation>(search->second), &mappedData);
		assert(result >= 0);
		memcpy(mappedData, data_pointer, data_size_in_bytes);
		vmaUnmapMemory(mVmaAllocator, std::get<VmaAllocation>(search->second));
	}
	else {
		uint8_t* mappedMemory = static_cast<uint8_t*>(mDevice.mapMemory(std::get<vk::UniqueDeviceMemory>(search->second).get(), 0, static_cast<vk::DeviceSize>(data_size_in_bytes)));
		mappedMemory += buffer_offset_in_bytes;
		memcpy(mappedMemory, data_pointer, data_size_in_bytes);
		mDevice.unmapMemory(std::get<vk::UniqueDeviceMemory>(search->second).get());
	}
#else
	uint8_t* mappedMemory = static_cast<uint8_t*>(mDevice.mapMemory(search->second.get(), 0, static_cast<vk::DeviceSize>(data_size_in_bytes)));
	mappedMemory += buffer_offset_in_bytes;
	memcpy(mappedMemory, data_pointer, data_size_in_bytes);
	mDevice.unmapMemory(search->second.get());
#endif
}

/*!
 * Create a new host coherent buffer on the GPU, upload the supplied data from the vector, and return the buffer handle.
 *
 * @param data Pointer to the data to upload to the GPU.
 * @param size Size of the data in bytes.
 * @param usageFlags Usage flags to use when createing the buffer.
 * @return The handle of the newly generated buffer.
 */
VkBuffer vklCreateHostCoherentBufferAndUploadData(const void* data, size_t size, VkBufferUsageFlags usageFlags) {
    VkBuffer result {};
    result = vklCreateHostCoherentBufferWithBackingMemory(
            static_cast<VkDeviceSize>(size),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usageFlags
    );
    vklCopyDataIntoHostCoherentBuffer(result, data, size);
    return result;
}

const char* vklRequiredInstanceExtensions[] = {
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};

const char** vklGetRequiredInstanceExtensions(uint32_t* out_count)
{
	*out_count = sizeof(vklRequiredInstanceExtensions) / sizeof(const char*);
	return vklRequiredInstanceExtensions;
}

void vklBindDescriptorSetToPipeline(VkDescriptorSet descriptor_set, VkPipeline pipeline)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	if (mSingleUseCommandBuffers.empty()) {
		VKL_EXIT_WITH_ERROR("There are no command buffers to record commands into. Have you called vklStartRecordingCommands() beforehand?");
	}
	auto& cb = mSingleUseCommandBuffers.back().get();

	auto searchPl = mPipelineLayouts.find(pipeline);
	if (mPipelineLayouts.end() == searchPl) {
		VKL_EXIT_WITH_ERROR("Couldn't find the VkPipeline passed to vklBindDescriptorSetToPipeline. Is it a valid handle and has it been created with vklCreateGraphicsPipeline(...)?");
	}
	
	auto dset = vk::DescriptorSet{ descriptor_set };
	auto pipe = vk::Pipeline{ pipeline };
	auto pipeLayout = std::get<vk::UniquePipelineLayout>(searchPl->second).get();

	cb.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics, pipeLayout,
		0u, 1u, &dset, // <--- Bind the actual descriptors to the pipeline
		0u, nullptr
	);
}

VkPipelineLayout vklGetLayoutForPipeline(VkPipeline pipeline)
{
	auto searchPl = mPipelineLayouts.find(pipeline);
	if (mPipelineLayouts.end() == searchPl) {
		VKL_EXIT_WITH_ERROR("Couldn't find the VkPipeline passed to vklBindDescriptorSetToPipeline. Is it a valid handle and has it been created with vklCreateGraphicsPipeline(...)?");
	}
	return static_cast<VkPipelineLayout>(std::get<vk::UniquePipelineLayout>(searchPl->second).get());
}

bool vklInitFramework(VkInstance vk_instance, VkSurfaceKHR vk_surface, VkPhysicalDevice vk_physical_device, VkDevice vk_device, VkQueue vk_queue, const VklSwapchainConfig& swapchain_config)
{
	if (VK_NULL_HANDLE == vk_instance) {
		VKL_EXIT_WITH_ERROR("Invalid VkInstance passed to vklInitFramework");
	}
	if (VK_NULL_HANDLE == vk_surface) {
		VKL_EXIT_WITH_ERROR("Invalid VkSurfaceKHR passed to vklInitFramework");
	}
	if (VK_NULL_HANDLE == vk_physical_device) {
		VKL_EXIT_WITH_ERROR("Invalid VkPhysicalDevice passed to vklInitFramework");
	}
	if (VK_NULL_HANDLE == vk_device) {
		VKL_EXIT_WITH_ERROR("Invalid VkDevice passed to vklInitFramework");
	}
	if (VK_NULL_HANDLE == vk_queue) {
		VKL_EXIT_WITH_ERROR("Invalid VkQueue passed to vklInitFramework");
	}
	if (VkSwapchainKHR{} == swapchain_config.swapchainHandle) {
		VKL_EXIT_WITH_ERROR("Invalid VkSwapchainKHR passed to vklInitFramework");
	}
	if (swapchain_config.imageExtent.width == 0 || swapchain_config.imageExtent.height == 0) {
		VKL_EXIT_WITH_ERROR("Invalid VkExtent2D passed to vklInitFramework through VklSwapchainConfig::imageExtent");
	}
	if (swapchain_config.imageExtent.width < 128 || swapchain_config.imageExtent.height < 128) {
		VKL_EXIT_WITH_ERROR("VkExtent2D passed to vklInitFramework through VklSwapchainConfig::imageExtent are too small (less than 128)");
	}
	if (swapchain_config.swapchainImages.empty()) {
		VKL_EXIT_WITH_ERROR("No data about swapchain images passed to vklInitFramework through VklSwapchainConfig::swapchainImages");
	}
	for (int i = 0; i < swapchain_config.swapchainImages.size(); ++i) {
		if (swapchain_config.swapchainImages[i].colorAttachmentImageDetails.imageHandle == VK_NULL_HANDLE) {
			VKL_EXIT_WITH_ERROR("No/invalid color attachment image details passed to vklInitFramework through VklSwapchainConfig::swapchainImages[" + std::to_string(i) + "]::colorAttachmentImageDetails");
		}
	}
	for (int i = 0; i < swapchain_config.swapchainImages.size(); ++i) {
		auto imageDetails = std::vector<VklSwapchainImageDetails>{ swapchain_config.swapchainImages[i].colorAttachmentImageDetails, swapchain_config.swapchainImages[i].depthAttachmentImageDetails };
		for (int j = 0; j < imageDetails.size(); ++j) {
			if (VK_NULL_HANDLE == imageDetails[j].imageHandle) {
				continue;
			}
			if (VkFormat{} == imageDetails[j].imageFormat) {
				VKL_EXIT_WITH_ERROR("Invalid VkFormat passed to vklInitFramework through VklSwapchainConfig::swapchainImages[" + std::to_string(i) + "]::imageDetails[" + std::to_string(j) + "]::imageFormat");
			}
			if (VkImageUsageFlags{} == imageDetails[j].imageUsage) {
				VKL_EXIT_WITH_ERROR("Invalid VkImageUsageFlags passed to vklInitFramework through VklSwapchainConfig::swapchainImages[" + std::to_string(i) + "]::imageDetails[" + std::to_string(j) + "]::imageUsage");
			}
		}
	}

	// Switch to Vulkan-Hpp (can't stand the C interface):
	mInstance = vk::Instance{ vk_instance };
	mSurface = vk::SurfaceKHR{ vk_surface };
	mPhysicalDevice = vk::PhysicalDevice{ vk_physical_device };
	mDevice = vk::Device{ vk_device };
	mDispatchLoader = vk::DispatchLoaderStatic();
	mQueue = vk::Queue{ vk_queue };
	mSwapchainConfig = swapchain_config;

	// Create a DYNAMIC DISPATCH LOADER:
	mDynamicDispatch = vk::DispatchLoaderDynamic{ static_cast<VkInstance>(mInstance), vkGetInstanceProcAddr };
	
	// Test instance and add DEBUG UTILS MESSENGER:
	mDebugUtilsMessenger = mInstance.createDebugUtilsMessengerEXT(vk::DebugUtilsMessengerCreateInfoEXT{
		vk::DebugUtilsMessengerCreateFlagsEXT{},
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
		DebugUtilsMessengerCallback, nullptr
	}, nullptr, mDynamicDispatch);

	// See if we can get some information about the surface:
	auto surfaceCapabilities = mPhysicalDevice.getSurfaceCapabilitiesKHR(mSurface);

	// Get swapchain image extents:
	if (swapchain_config.imageExtent.width != surfaceCapabilities.currentExtent.width || swapchain_config.imageExtent.height != surfaceCapabilities.currentExtent.height) {
		std::cout << "WARNING: Swapchain config's extents[" << swapchain_config.imageExtent.width << "x" << swapchain_config.imageExtent.height << "] do not match the surface capabilities' extents[" << surfaceCapabilities.currentExtent.width << "x" << surfaceCapabilities.currentExtent.height << "]" << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << "\n";
	}
	
	// Wrap swapchain images with IMAGE VIEWS and prepare data for RENDERPASS:
	mSwapchainImageViews.resize(mSwapchainConfig.swapchainImages.size());

	std::vector<vk::AttachmentDescription> attachmentDescriptions;
	// Layout transitions for all color attachments in here:
	std::vector<vk::AttachmentReference> colorAttachmentsInSubpass0;
	// Layout transitions for all depth attachments in here:
	std::vector<vk::AttachmentReference> depthAttachmentsInSubpass0;

	for (size_t i = 0; i < mSwapchainConfig.swapchainImages.size(); ++i) {
		std::vector<VklSwapchainImageDetails> attachments_0;
		if (mSwapchainConfig.swapchainImages[0].colorAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) { attachments_0.push_back(mSwapchainConfig.swapchainImages[0].colorAttachmentImageDetails); }
		if (mSwapchainConfig.swapchainImages[0].depthAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) { attachments_0.push_back(mSwapchainConfig.swapchainImages[0].depthAttachmentImageDetails); }

		auto hasColorAttachment = mSwapchainConfig.swapchainImages[i].colorAttachmentImageDetails.imageHandle != VK_NULL_HANDLE;
		auto hasDepthAttachment = mSwapchainConfig.swapchainImages[i].depthAttachmentImageDetails.imageHandle != VK_NULL_HANDLE;
		std::vector<VklSwapchainImageDetails> attachments_i;
		if (hasColorAttachment) { attachments_i.push_back(mSwapchainConfig.swapchainImages[i].colorAttachmentImageDetails); }
		if (hasDepthAttachment) { attachments_i.push_back(mSwapchainConfig.swapchainImages[i].depthAttachmentImageDetails); }
		mSwapchainImageViews[i] = std::vector<vk::ImageView>(attachments_i.size());

		// Sanity check:
		if ((mSwapchainConfig.swapchainImages[0].colorAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) != (mSwapchainConfig.swapchainImages[i].colorAttachmentImageDetails.imageHandle != VK_NULL_HANDLE)) {
			VKL_EXIT_WITH_ERROR(std::string("If one VklSwapchainFramebufferComposition entry has a valid color image handle set, all other VklSwapchainFramebufferComposition entries must have valid color image handles set, too. However, swapchainImages[0] has a ")
                                + ((mSwapchainConfig.swapchainImages[0].colorAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) ? "valid" : "invalid")
                                + " handle, while swapchainImages[" + std::to_string(i) + "] has a "
                                + ((mSwapchainConfig.swapchainImages[i].colorAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) ? "valid handle" : "invalid handle"));
		}
		if ((mSwapchainConfig.swapchainImages[0].depthAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) != (mSwapchainConfig.swapchainImages[i].depthAttachmentImageDetails.imageHandle != VK_NULL_HANDLE)) {
			VKL_EXIT_WITH_ERROR(std::string("If one VklSwapchainFramebufferComposition entry has a valid depth image handle set, all other VklSwapchainFramebufferComposition entries must have valid depth image handles set, too. However, swapchainImages[0] has a ")
                                + ((mSwapchainConfig.swapchainImages[0].depthAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) ? "valid" : "invalid")
                                + " handle, while swapchainImages[" + std::to_string(i) + "]  has a "
                                + ((mSwapchainConfig.swapchainImages[i].depthAttachmentImageDetails.imageHandle != VK_NULL_HANDLE) ? "valid handle" : "invalid handle"));
		}
		if (attachments_0.size() != attachments_i.size()) {
			VKL_EXIT_WITH_ERROR("attachments_0.size() != attachments_i.size()");
		}

		mClearValues.emplace_back();
		auto& currentClearValues = mClearValues.back();

		for (size_t j = 0; j < attachments_i.size(); ++j) {

			// Sanity checks:
			if (attachments_i[j].imageFormat != attachments_0[j].imageFormat) {
				VKL_EXIT_WITH_ERROR("Corresponding VklSwapchainImageDetails::imageFormat entries must be set to the same formats! However, element[" + std::to_string(i) + ", " + std::to_string(j) + "] is set to "
                                    + vk::to_string(static_cast<vk::Format>(attachments_i[j].imageFormat)) + ", while element[0, " + std::to_string(j) + "] is set to " + vk::to_string(static_cast<vk::Format>(attachments_0[j].imageFormat)));
			}
			if (attachments_i[j].imageUsage != attachments_0[j].imageUsage) {
				VKL_EXIT_WITH_ERROR("Corresponding VklSwapchainImageDetails::imageUsage entries must be set to the same values! However, element[" + std::to_string(i) + ", " + std::to_string(j) + "] is set to "
                                    + vk::to_string(static_cast<vk::ImageUsageFlags>(attachments_i[j].imageUsage)) + ", while element[0, " + std::to_string(j) + "] is set to " + vk::to_string(static_cast<vk::ImageUsageFlags>(attachments_0[j].imageUsage)));
			}

			// Create the views:
			if ((VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & attachments_i[j].imageUsage) != 0) {
				// Create a view for a depth buffer:
				mSwapchainImageViews[i][j] = mDevice.createImageView(vk::ImageViewCreateInfo{
					{}, vk::Image{attachments_i[j].imageHandle },
					vk::ImageViewType::e2D, static_cast<vk::Format>(attachments_i[j].imageFormat),
					vk::ComponentMapping{},
					vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u }
				});
			}
			else {
				// Create a view for a color buffer:
				mSwapchainImageViews[i][j] = mDevice.createImageView(vk::ImageViewCreateInfo{
					{}, vk::Image{ attachments_i[j].imageHandle },
					vk::ImageViewType::e2D, static_cast<vk::Format>(attachments_i[j].imageFormat),
					vk::ComponentMapping{},
					vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u }
				});
			}
			
			// Gather information for the renderpass already:
			if (0 == i) {
				auto curAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());

				attachmentDescriptions.emplace_back(vk::AttachmentDescription{}
					.setFormat(static_cast<vk::Format>(attachments_i[j].imageFormat))
					.setLoadOp(vk::AttachmentLoadOp::eClear)		// What do do with the image when the renderpass starts? => Make sure that we have cleared the content of previous frames!
					.setStoreOp( // What to do with the image when the renderpass has finished? => We don't need the depth buffer for anything afterwards.
						(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & attachments_i[j].imageUsage) != 0
						? vk::AttachmentStoreOp::eDontCare
						: vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eUndefined)	// When the renderpass starts, in which layout will the image be? => We don't care since we're clearing it.
					.setFinalLayout( // When the renderpass finishes, in which layout shall the image be transfered? => The image shall be presented directly afterwards. 
						(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & attachments_i[j].imageUsage) != 0
						? vk::ImageLayout::eDepthStencilAttachmentOptimal // When the renderpass finishes, in which layout shall the image be transferred? => It will be in eDepthStencilAttachmentOptimal layout anyways.
						: vk::ImageLayout::ePresentSrcKHR)
					.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
					.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
				);
				
				if ((VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & attachments_i[j].imageUsage) != 0) {
					depthAttachmentsInSubpass0.emplace_back(curAttachmentIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal); // Describes the index (w.r.t. attachmentDescriptions) and the desired layout of the depth attachment for subpass 0
				}
				else {
					colorAttachmentsInSubpass0.emplace_back(curAttachmentIndex, vk::ImageLayout::eColorAttachmentOptimal); // Describes the index (w.r.t. attachmentDescriptions) and the desired layout of the color attachment for subpass 0
				}
			}

			currentClearValues.emplace_back(*reinterpret_cast<vk::ClearValue*>(&attachments_i[j].clearValue));
		}
	}

	mHasDepthAttachments = !depthAttachmentsInSubpass0.empty();

	// Create the RENDERPASS:
	// ad 2) Describe per subpass for each attachment how it is going to be used, and into which layout it shall be transferred
	auto subpassDescription = vk::SubpassDescription{}
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics) // Despite looking as if it was configurable, only eGraphics is valid/supported
		.setColorAttachmentCount(static_cast<uint32_t>(colorAttachmentsInSubpass0.size()))
		.setPColorAttachments(colorAttachmentsInSubpass0.data());
	if (mHasDepthAttachments) {
		subpassDescription.setPDepthStencilAttachment(depthAttachmentsInSubpass0.data());
	}

	// In any case, prepare for potential device transfers (we wouldn't need it for host coherent buffers, which are made available on queue submission)
	mSrcStages0 = vk::PipelineStageFlagBits::eTransfer;
	mSrcAccess0 = vk::AccessFlagBits::eTransferWrite;
	// In any case, we must wait for such potential transfers in fragment shaders, where we're using the buffers
	mDstStages0 = vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eColorAttachmentOutput;
	mDstAccess0 = vk::AccessFlagBits::eShaderRead            | vk::AccessFlagBits::eColorAttachmentWrite        ;
	// If we have depth attachments, prepare for the case where one single depth image is used for all framebuffers in flight:
	// (For further details, see here: https://stackoverflow.com/questions/62371266/why-is-a-single-depth-buffer-sufficient-for-this-vulkan-swapchain-render-loop/62398311#62398311)
	if (mHasDepthAttachments) {
		mSrcStages0 |= (vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests);
		mSrcAccess0 |= (vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		mDstStages0 |= (vk::PipelineStageFlagBits::eEarlyFragmentTests  | vk::PipelineStageFlagBits::eLateFragmentTests   );
		mDstAccess0 |= (vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
	}
	// We don't really need to synchronize on the COLOR_ATTACHMENT_OUTPUT stage since actually our fences ensure that we do not reuse the same swap chain image.
	// Therefore, this should be fine.

	// ad 3) Describe execution and memory dependencies (just in the same way as with pipeline barriers).
	//        In this case, we only have external dependencies: One with whatever comes before we are using
	//        this renderpass, and another with whatever comes after this renderpass in a queue.
	std::array<vk::SubpassDependency, 2> subpassDependencies{
		vk::SubpassDependency{}
			// Establish proper dependencies with whatever comes before (which is the imageAvailableSemaphore wait and then the command buffer that copies an explosion image to the swapchain image):
			.setSrcSubpass(VK_SUBPASS_EXTERNAL) /* -> */ .setDstSubpass(0u)
			.setSrcStageMask(mSrcStages0) /* -> */ .setDstStageMask(mDstStages0)
			.setSrcAccessMask(mSrcAccess0) /* -> */ .setDstAccessMask(mDstAccess0)
	,	vk::SubpassDependency{}
			// Establish proper dependencies with whatever comes after (which is the renderFinishedSemaphore signal):
			.setSrcSubpass(0u) /* -> */ .setDstSubpass(VK_SUBPASS_EXTERNAL)
			//     Execution may continue as soon as the eColorAttachmentOutput stage is done.
			//     However, nothing must really wait on that stage, because afterwards comes the semaphore. Hence, eBottomOfPipe.
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput) /* -> */ .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
			//     The graphics pipeline is performing eColorAttachmentWrites. These need to be made available.
			//     We don't have to make them visible to anything, because the semaphore performs a full memory barrier anyways. 
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite) /* -> */ .setDstAccessMask(vk::AccessFlags{})
	};

	auto renderpassCreateInfo = vk::RenderPassCreateInfo{}
		.setAttachmentCount(static_cast<uint32_t>(attachmentDescriptions.size()))
		.setPAttachments(attachmentDescriptions.data())
		.setSubpassCount(1u)
		.setPSubpasses(&subpassDescription)
		.setDependencyCount(static_cast<uint32_t>(subpassDependencies.size()))
		.setPDependencies(subpassDependencies.data());
	mRenderpass = mDevice.createRenderPassUnique(renderpassCreateInfo, nullptr, mDispatchLoader);

	// Create the FRAMEBUFFERS
	mFramebuffers.reserve(mSwapchainImageViews.size());
	for (const auto& set : mSwapchainImageViews) {
		auto framebufferCreateInfo = vk::FramebufferCreateInfo{}
			.setRenderPass(mRenderpass.get())
			.setAttachmentCount(static_cast<uint32_t>(set.size()))
			.setPAttachments(set.data())
			.setWidth(mSwapchainConfig.imageExtent.width)
			.setHeight(mSwapchainConfig.imageExtent.height)
			.setLayers(1u);

		mFramebuffers.push_back(mDevice.createFramebufferUnique(framebufferCreateInfo, nullptr, mDispatchLoader));
	}

	// Create SEMAPHORES and FENCES, and also prepare the safety-vector of FENCES
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		mImageAvailableSemaphores[i] = mDevice.createSemaphoreUnique(vk::SemaphoreCreateInfo{}, nullptr, mDispatchLoader);
		mRenderFinishedSemaphores[i] = mDevice.createSemaphoreUnique(vk::SemaphoreCreateInfo{}, nullptr, mDispatchLoader);
		mSyncHostWithDeviceFence[i] = mDevice.createFenceUnique(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled), nullptr, mDispatchLoader);
	}
	mImagesInFlightFenceIndices.resize(mFramebuffers.size(), -1);

	mFrameId = -1;
	// We have to make sure that not more than #CONCURRENT_FRAMES are in flight at the same time. We can use fences to ensure that. 
	mFrameInFlightIndex = -1; // Initialize
	
#ifdef USE_GLSLANG
	glslang_initialize_process();
#endif
	mBasicPipeline = vk::Pipeline{ vklCreateGraphicsPipeline(VklGraphicsPipelineConfig{
		// Vertex Shader from memory:
			"#version 450\n"
			"layout(location = 0) in vec3 position;\n"
			"void main() {\n"
			"    gl_Position = vec4(position.x, -position.y, position.z, 1);\n"
			"}\n",
		// Fragment shader from memory:
			"#version 450\n"
			"layout(location = 0) out vec4 color; \n"
			"void main() {  \n"
			"    color = vec4(1, 0, 0, 1); \n"
			"}\n",
		// Further config parameters:
		{
			VkVertexInputBindingDescription { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX }
		},
		{
			VkVertexInputAttributeDescription { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0u }
		},
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_NONE,
		{ /* no descriptors */ }
	}, /* load shaders from memory: */ true) };

	// Create a default COMMAND POOL which command buffers will be allocated from during vklStartRecordingCommands()
	mCommandPool = mDevice.createCommandPoolUnique(vk::CommandPoolCreateInfo{ vk::CommandPoolCreateFlagBits::eTransient }, nullptr, mDispatchLoader);
	
	mFrameworkInitialized = true;
	return mFrameworkInitialized;
}

#ifdef VKL_HAS_VMA
bool vklInitFramework(VkInstance vk_instance, VkSurfaceKHR vk_surface, VkPhysicalDevice vk_physical_device,
                      VkDevice vk_device, VkQueue vk_queue, const VklSwapchainConfig &swapchain_config,
                      VmaAllocator vma_allocator)
{
	bool err = vklInitFramework(vk_instance, vk_surface, vk_physical_device, vk_device, vk_queue, swapchain_config);

	if (VmaAllocator{} == vma_allocator) {
		VKL_EXIT_WITH_ERROR("Invalid VmaAllocator handle passed to vklInitFramework");
	}
	mVmaAllocator = vma_allocator;
	return err;
}
#endif 

bool vklFrameworkInitialized()
{
	return mFrameworkInitialized;
}

void vklDestroyFramework()
{
	mDevice.waitIdle();
	mFrameworkInitialized = false;

	// DESTROOOOOOOOY:

	mSingleUseCommandBuffers.clear();

	mPipelineLayouts.clear();

	mCommandPool.reset();
	mDevice.destroyPipeline(mBasicPipeline);
#ifdef USE_GLSLANG
	glslang_finalize_process();
#endif
	mImagesInFlightFenceIndices.clear();
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		mSyncHostWithDeviceFence[i].reset();
		mRenderFinishedSemaphores[i].reset();
		mImageAvailableSemaphores[i].reset();
	}
	mFramebuffers.clear();
	mRenderpass.reset();
	for (const auto& set : mSwapchainImageViews) {
		for (const auto& view : set) {
			mDevice.destroyImageView(view);
		}
	}
	mSwapchainImageViews.clear();

	mInstance.destroyDebugUtilsMessengerEXT(mDebugUtilsMessenger, nullptr, mDynamicDispatch);
	mDebugUtilsMessenger = nullptr;
}

double vklWaitForNextSwapchainImage()
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Advance the frame ID:
	++mFrameId;
	mFrameInFlightIndex = mFrameId % CONCURRENT_FRAMES;

	// Just out of curiosity, measure the wait time:
	auto t0 = glfwGetTime();

	// Wait for the fence of the current image before reusing the same image available semaphore (as we have used #CONCURRENT_FRAMES in the past)
	vk::Result returnCode = mDevice.waitForFences(1u, &mSyncHostWithDeviceFence[mFrameInFlightIndex].get(), VK_TRUE, std::numeric_limits<uint64_t>::max()); // Wait up to forever
	VKL_CHECK_VULKAN_ERROR(static_cast<VkResult>(returnCode));

	returnCode = mDevice.resetFences(1u, &mSyncHostWithDeviceFence[mFrameInFlightIndex].get());
	VKL_CHECK_VULKAN_ERROR(static_cast<VkResult>(returnCode));

	// Keep house with the in-flight images:
	for (auto& mapping : mImagesInFlightFenceIndices) { // However, we don't know which index this fence had been mapped to => we have to search
		if (mFrameInFlightIndex == mapping) {
			mapping = -1;
			break;
		}
	}

	// Get the next image from the swap chain:
	mCurrentSwapChainImageIndex = mDevice.acquireNextImageKHR(vk::SwapchainKHR{ mSwapchainConfig.swapchainHandle }, std::numeric_limits<uint64_t>::max(), mImageAvailableSemaphores[mFrameInFlightIndex].get(), nullptr).value;
	// Safety-check on the returned image index:
	if (mImagesInFlightFenceIndices[mCurrentSwapChainImageIndex] >= 0) {
		// it is set => must perform an extra wait
		returnCode = mDevice.waitForFences(1u, &mSyncHostWithDeviceFence[mImagesInFlightFenceIndices[mCurrentSwapChainImageIndex]].get(), VK_TRUE, std::numeric_limits<uint64_t>::max()); // Wait up to forever
		VKL_CHECK_VULKAN_ERROR(static_cast<VkResult>(returnCode));
		// But do not reset! Otherwise we will wait forever at the next waitForFences that will happen for sure.
	}

	// Submit a "fake" work package to the queue in order to wait for the image to become available before starting to render into it:
	mQueue.submit({ vk::SubmitInfo{}
		.setWaitSemaphoreCount(1u)
		// Wait for the image to become available:
		.setPWaitSemaphores(&mImageAvailableSemaphores[mFrameInFlightIndex].get())
		.setPWaitDstStageMask(&mDstStages0) // It's the same destination stages that must wait on the image to become available.
		.setCommandBufferCount(0u) // Submit ZERO command buffers :O
		// We don't signal anything here:
		.setSignalSemaphoreCount(0u)
	});

	auto t1 = glfwGetTime();
	return t1 - t0;
}

void vklPresentCurrentSwapchainImage()
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Submit yet another "fake" (but still) work package to signal the end of the rendering:
	mQueue.submit({ vk::SubmitInfo()
		// Don't wait for any semaphores:
		.setWaitSemaphoreCount(0u)
		.setCommandBufferCount(0u) // Submit ZERO command buffers :O
		// Signal a semaphore which we need for presentation:
		.setSignalSemaphoreCount(1u)
		.setPSignalSemaphores(&mRenderFinishedSemaphores[mFrameInFlightIndex].get())
		// Also signal a fence so that the CPU does not run ahead of the GPU:
	}, mSyncHostWithDeviceFence[mFrameInFlightIndex].get());

	// Now present the image as soon as the render finished semaphore has been signaled:
	auto swapchainHandle = vk::SwapchainKHR{ mSwapchainConfig.swapchainHandle };
	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1u)
		.setPWaitSemaphores(&mRenderFinishedSemaphores[mFrameInFlightIndex].get())
		.setSwapchainCount(1u)
		.setPSwapchains(&swapchainHandle)
		.setPImageIndices(&mCurrentSwapChainImageIndex);
	
	vk::Result returnCode = mQueue.presentKHR(presentInfo);
	VKL_CHECK_VULKAN_ERROR(static_cast<VkResult>(returnCode));

	mImagesInFlightFenceIndices[mCurrentSwapChainImageIndex] = mFrameInFlightIndex;
}

void vklStartRecordingCommands()
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Clean up old command buffers:
	auto numToErase = static_cast<size_t>(std::max(int{ 0 }, static_cast<int>(mSingleUseCommandBuffers.size()) - std::max(static_cast<int>(mSwapchainImageViews.size()), int{ CONCURRENT_FRAMES })));
	assert(numToErase <= mSingleUseCommandBuffers.size());
	assert(numToErase <  mSingleUseCommandBuffers.size() || mFrameId < 10); // <-- Must be strictly smaller in later frames
	mSingleUseCommandBuffers.erase(std::begin(mSingleUseCommandBuffers), std::begin(mSingleUseCommandBuffers) + numToErase);

	// Create a new command buffer for this frame:
	auto tmp = mDevice.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
			mCommandPool.get(),
			vk::CommandBufferLevel::ePrimary,
			1u
		}, 
		mDispatchLoader
	);
	assert(!tmp.empty());

	mSingleUseCommandBuffers.push_back(std::move(tmp[0]));
	auto& cb = mSingleUseCommandBuffers.back().get();
	
	// Start recording:
	cb.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	cb.beginRenderPass(vk::RenderPassBeginInfo{
		mRenderpass.get(), mFramebuffers[mCurrentSwapChainImageIndex].get(),
		vk::Rect2D{vk::Offset2D{0, 0}, mSwapchainConfig.imageExtent},
		static_cast<uint32_t>(mClearValues[mCurrentSwapChainImageIndex].size()), mClearValues[mCurrentSwapChainImageIndex].data()
		}, vk::SubpassContents::eInline);
}

void vklEndRecordingCommands()
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	if (mSingleUseCommandBuffers.empty()) {
		VKL_EXIT_WITH_ERROR("There are no command buffers which could be recording.Have you called vklStartRecordingCommands beforehand?");
	}
	const auto& cb = mSingleUseCommandBuffers.back().get();
	
	cb.endRenderPass();

	// Stop recording:
	cb.end();

	mQueue.submit({ vk::SubmitInfo{}
						.setCommandBufferCount(1u)
						.setPCommandBuffers(&cb)
					});
}

uint32_t vklGetCurrentSwapChainImageIndex()
{
	return mCurrentSwapChainImageIndex;
}
uint32_t vklGetNumFramebuffers()
{
	return static_cast<uint32_t>(mFramebuffers.size());
}
uint32_t vklGetNumClearValues()
{
  return static_cast<uint32_t>(mClearValues.size());
}
VkFramebuffer vklGetFramebuffer(uint32_t i)
{
	if (i >= mFramebuffers.size()) {
		VKL_EXIT_WITH_ERROR("The given index[" + std::to_string(i) + "] is larger than the number of available framebuffers[" + std::to_string(mFramebuffers.size()) + "]");
	}
	return static_cast<VkFramebuffer>(mFramebuffers[i].get());
}
VkFramebuffer vklGetCurrentFramebuffer()
{
	return vklGetFramebuffer(vklGetCurrentSwapChainImageIndex());
}
VkRenderPass vklGetRenderpass()
{
	return static_cast<VkRenderPass>(mRenderpass.get());
}
VkCommandBuffer vklGetCurrentCommandBuffer()
{
	if(mSingleUseCommandBuffers.empty()) {
		VKL_EXIT_WITH_ERROR("There are no command buffers. Have you called vklStartRecordingCommands beforehand?");
	}
	const auto& cb = mSingleUseCommandBuffers.back().get();
	return static_cast<VkCommandBuffer>(cb);
}

VkPipeline vklGetBasicPipeline()
{
    return static_cast<VkPipeline>(mBasicPipeline);
}

VkDevice vklGetDevice()
{
  return static_cast<VkDevice>(mDevice);
}

VkImage vklCreateDeviceLocalImageWithBackingMemory(VkPhysicalDevice physical_device, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage_flags, uint32_t array_layers, VkImageCreateFlags flags)
{
	auto createInfo = vk::ImageCreateInfo{}
		.setFlags(static_cast<vk::ImageCreateFlagBits>(flags))
		.setImageType(vk::ImageType::e2D)
		.setExtent({ width, height, 1u })
		.setMipLevels(static_cast<uint32_t>(1 + std::floor(std::log2(std::max(width, height)))))
		.setArrayLayers(array_layers)
		.setFormat(static_cast<vk::Format>(format))
		.setTiling(vk::ImageTiling::eOptimal)			// We just create all images in optimal tiling layout
		.setInitialLayout(vk::ImageLayout::eUndefined)	// Initially, the layout is undefined
		.setUsage(static_cast<vk::ImageUsageFlags>(usage_flags))
		.setSamples(vk::SampleCountFlagBits::e1)
		.setSharingMode(vk::SharingMode::eExclusive);

#ifdef VKL_HAS_VMA
	if (vklHasVmaAllocator()) {
		VmaAllocationCreateInfo vmaImageCreateInfo = {};
		vmaImageCreateInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
		vmaImageCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkImage imageFromVma;
		VmaAllocation vmaAllocation;
		vmaCreateImage(mVmaAllocator, &static_cast<const VkImageCreateInfo&>(createInfo), &vmaImageCreateInfo, &imageFromVma, &vmaAllocation, nullptr);
		mImagesWithBackingMemory[imageFromVma] = std::move(vmaAllocation);
		return imageFromVma;
	}
#endif

	auto image = vk::Device{ device }.createImage(createInfo);

	auto memoryRequirements = vk::Device{ device }.getImageMemoryRequirements(image);

	auto memoryAllocInfo = vk::MemoryAllocateInfo{}
		.setAllocationSize(memoryRequirements.size)
		.setMemoryTypeIndex([&]() {
			// Get memory types supported by the physical device:
			auto memoryProperties = vk::PhysicalDevice{ physical_device }.getMemoryProperties();

			// In search for a suitable memory type INDEX:
			int selectedMemIndex = -1;
			vk::DeviceSize selectedHeapSize = 0;
			for (int i = 0; i < static_cast<int>(memoryProperties.memoryTypeCount); ++i) {

				// Is this kind of memory suitable for our image?
				const auto bitmask = memoryRequirements.memoryTypeBits;
				const auto bit = 1 << i;
				if (0 == (bitmask & bit)) {
					continue; // => nope
				}

				// Does this kind of memory support our usage requirements?

				// In contrast to our host-coherent buffers, we just assume that we want all our images to live in device memory:
				if ((memoryProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags{}) {
					// Would support => now select the one with the largest heap:
					const auto heapSize = memoryProperties.memoryHeaps[memoryProperties.memoryTypes[i].heapIndex].size;
					if (heapSize > selectedHeapSize) {
						// We have a new king:
						selectedMemIndex = i;
						selectedHeapSize = heapSize;
					}
				}
			}

			if (-1 == selectedMemIndex) {
				VKL_EXIT_WITH_ERROR(std::string("ERROR: Couldn't find suitable memory for image, requirements[") + std::to_string(memoryRequirements.alignment) + ", " + std::to_string(memoryRequirements.memoryTypeBits) + ", " + std::to_string(memoryRequirements.size) + "]");
			}

			// all good, we found a suitable memory index:
			return static_cast<uint32_t>(selectedMemIndex);
		}());

	auto memory = vk::Device{ device }.allocateMemoryUnique(memoryAllocInfo, nullptr, mDispatchLoader);

	vk::Device{ device }.bindImageMemory(image, memory.get(), 0);

	// Remember the assignment:
	mImagesWithBackingMemory[static_cast<VkImage>(image)] = std::move(memory);

	return static_cast<VkImage>(image);
}

VkImage vklCreateDeviceLocalImageWithBackingMemory(VkPhysicalDevice physical_device, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage_flags)
{
	return vklCreateDeviceLocalImageWithBackingMemory(physical_device, device, width, height, format, usage_flags, /* one layer: */ 1u, /* no flags: */{});
}

VkImage vklCreateDeviceLocalImageWithBackingMemory(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage_flags)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	return vklCreateDeviceLocalImageWithBackingMemory(static_cast<VkPhysicalDevice>(mPhysicalDevice), static_cast<VkDevice>(mDevice), width, height, format, usage_flags);
}

VkImage vklCreateDeviceLocalImageWithBackingMemory(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage_flags, uint32_t array_layers, VkImageCreateFlags flags)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	return vklCreateDeviceLocalImageWithBackingMemory(static_cast<VkPhysicalDevice>(mPhysicalDevice), static_cast<VkDevice>(mDevice), width, height, format, usage_flags, array_layers, flags);
}

void vklDestroyDeviceLocalImageAndItsBackingMemory(VkImage image)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	if (VkImage{} == image) {
		VKL_EXIT_WITH_ERROR("Invalid image handle passed to vklDestroyImageAndItsBackingMemory(...)");
	}

	bool resourceDestroyed = false;
	auto search = mImagesWithBackingMemory.find(image);
	if (mImagesWithBackingMemory.end() != search) {
#ifdef VKL_HAS_VMA
		if (vklHasVmaAllocator() && std::holds_alternative<VmaAllocation>(search->second)) {
			vmaDestroyImage(mVmaAllocator, image, std::get<VmaAllocation>(search->second));
			resourceDestroyed = true;
		}
#endif
		mImagesWithBackingMemory.erase(search);
	}
	else {
		VKL_WARNING("VkDeviceMemory for the given VkImage not found. Are you sure that you have created this buffer with vklCreateDeviceLocalImageWithBackingMemory(...)? Are you sure that you haven't already destroyed this VkImage?");
	}

	if (!resourceDestroyed) {
		mDevice.destroy(vk::Image{ image });
	}
}
#pragma endregion

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data)
{
	if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
	{
		std::cout << "\nERROR:   messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
	{
		std::cout << "\nWARNING: messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
	{
		std::cout << "\nINFO:    messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0)
	{
		std::cout << "\nVERBOSE: messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	return VK_FALSE;
}

#ifdef USE_GLI
// Internal helper function
std::tuple<VklImageInfo, gli::texture2d> loadDdsImageWithGli(const char* file, uint32_t level)
{
	gli::texture2d gliTex(gli::load(file));
	if (gliTex.empty()) {
		VKL_EXIT_WITH_ERROR(std::string("Unable to load DDS image file from path[") + file + "]");
	}

	if (level > gliTex.max_level()) {
		VKL_EXIT_WITH_ERROR(std::string("The specificed level[") + std::to_string(level) + "] is not available in the DDS image file at path[" + file + "]. Your are probably trying to load a level > 0 from a file which does not contain mipmap levels.");
	}

	const uint32_t width = gliTex.extent(static_cast<gli::texture2d::size_type>(level)).x;
	const uint32_t height = gliTex.extent(static_cast<gli::texture2d::size_type>(level)).y;
	auto gliFormat = gliTex.format();
	VkFormat vkFormat;

	switch (gliFormat) {
		// See "Khronos Data Format Specification": https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#S3TC
		// And Vulkan specification: https://www.khronos.org/registry/vulkan/specs/1.2-khr-extensions/html/chap42.html#appendix-compressedtex-bc
	case gli::format::FORMAT_RGB_DXT1_UNORM_BLOCK8:
		vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGB_DXT1_SRGB_BLOCK8:
		vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
		vkFormat = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
		vkFormat = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGBA_DXT3_UNORM_BLOCK16:
		vkFormat = VK_FORMAT_BC2_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGBA_DXT3_SRGB_BLOCK16:
		vkFormat = VK_FORMAT_BC2_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
		vkFormat = VK_FORMAT_BC3_SRGB_BLOCK;
		break;
	case gli::format::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
		vkFormat = VK_FORMAT_BC3_SRGB_BLOCK;
		break;
	default:
		VKL_EXIT_WITH_ERROR(std::string("Unable to load DDS image file [") + file + "] due to an unsupported format.");
	}

	return std::make_tuple(
		VklImageInfo{ vkFormat, VkExtent2D { width, height } },
		std::move(gliTex)
	);
}
#endif

VklImageInfo vklGetDdsImageLevelInfo(const char* file, uint32_t level)
{
#ifdef USE_GLI
	return std::get<VklImageInfo>(loadDdsImageWithGli(file, level));
#else
	VklImageInfo info;

	unsigned char header[124];

	FILE* fp;

	/* try to open the file */
	fp = fopen(file, "rb");
	if (fp == NULL)
	{
		VKL_EXIT_WITH_ERROR(std::string("Unable to load DDS image file from path[") + file + "]");
	}

	/* verify the type of file */
	char filecode[4];
	fread(filecode, 1, 4, fp);
	if (strncmp(filecode, "DDS ", 4) != 0)
	{
		fclose(fp);
		VKL_EXIT_WITH_ERROR(std::string("The given image file at path[") + file + "] does not seem to be in DDS format.");
	}

	/* get the surface desc */
	fread(&header, 124, 1, fp);

	auto height = *reinterpret_cast<unsigned*>(&(header[8]));
	auto width = *reinterpret_cast<unsigned*>(&(header[12]));
	info.extent.width = width;
	info.extent.height = height;

	unsigned int linearSize = *reinterpret_cast<unsigned*>(&(header[16]));
	unsigned int fourCC = *reinterpret_cast<unsigned*>(&(header[80]));

	/* close the file pointer */
	fclose(fp);

	switch (fourCC)
	{
		// Okay... looks like we're only deadling with RGBA formats (not RGB formats)
		//  => only set RGBA formats!
		// Furthermore, assume just UNORM, but could also be sRGB... who knows?!
		// TODO: Test if sRGB looks better than UNORM!
	case FOURCC_DXT1:
		info.imageFormat= VK_FORMAT_BC1_RGBA_UNORM_BLOCK; // TODO: maybe VK_FORMAT_BC1_RGBA_SRGB_BLOCK?
		break;
	case FOURCC_DXT3:
		info.imageFormat = VK_FORMAT_BC2_UNORM_BLOCK; // TODO: maybe VK_FORMAT_BC2_RGBA_SRGB_BLOCK? And are we sure about BC2? Could it be that it is BC3 (whatever BC3 is)?
		break;
	case FOURCC_DXT5:
		info.imageFormat = VK_FORMAT_BC3_UNORM_BLOCK; // TODO: maybe VK_FORMAT_BC3_RGBA_SRGB_BLOCK? And are we sure about BC3? Could it be that it is BC5 (whatever BC5 is)?
		break;
	default:
		VKL_EXIT_WITH_ERROR("Unable to determine the DDS file's format (seems to be neither DXT1, nor DXT3, nor DXT5)");
	}

	return info;
#endif
}

VklImageInfo vklGetDdsImageInfo(const char* file)
{
	return vklGetDdsImageLevelInfo(file, 0u);
}

VkBuffer vklLoadDdsImageFaceLevelIntoHostCoherentBuffer(const char* file, uint32_t face, uint32_t level)
{
	{ // Just checking if we are able to open the file:
		std::ifstream infile(file);
		if (infile.good()) {
			VKL_LOG("Loading DDS image file from path[" << file << "]...");
		}
		else { // Fail if model file could not be found:
			VKL_EXIT_WITH_ERROR("Unable to load file[" << file << "].");
		}
	}

#ifdef USE_GLI
	auto imageFace = static_cast<gli::texture2d::size_type>(static_cast<size_t>(face));
	auto imageLevel = static_cast<gli::texture2d::size_type>(static_cast<size_t>(level));

	auto gliTpl = loadDdsImageWithGli(file, static_cast<uint32_t>(imageLevel));
	const auto& gliTex = std::get<gli::texture2d>(gliTpl);

	imageLevel = glm::clamp(imageLevel, gliTex.base_level(), gliTex.max_level());
	
	auto bufsize = gliTex.size(imageLevel);
	auto buffer = gliTex.data(0, imageFace, imageLevel);
	auto host_coherent_buffer = vklCreateHostCoherentBufferWithBackingMemory(bufsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vklCopyDataIntoHostCoherentBuffer(host_coherent_buffer, buffer, bufsize);
#else
	unsigned char header[124];

	FILE* fp;

	/* try to open the file */
	fp = fopen(file, "rb");
	if (fp == NULL)
	{
		VKL_EXIT_WITH_ERROR(std::string("Unable to load DDS image file from path[") + file + "]");
	}

	/* verify the type of file */
	char filecode[4];
	fread(filecode, 1, 4, fp);
	if (strncmp(filecode, "DDS ", 4) != 0)
	{
		fclose(fp);
		VKL_EXIT_WITH_ERROR(std::string("The given image file at path[") + file + "] does not seem to be in DDS format.");
	}

	/* get the surface desc */
	fread(&header, 124, 1, fp);

	uint32_t height = *reinterpret_cast<uint32_t*>(&(header[8]));
	uint32_t width = *reinterpret_cast<uint32_t*>(&(header[12]));
	uint32_t linearSize = *reinterpret_cast<uint32_t*>(&(header[16]));
	uint32_t fourCC = *reinterpret_cast<uint32_t*>(&(header[80]));
	VkFormat format;

	unsigned char* buffer;
	unsigned int bufsize;
	/* how big is it going to be including all mipmaps? */
	bufsize = linearSize;
	//buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
	buffer = new unsigned char[bufsize];
	fread(buffer, 1, bufsize, fp);
	/* close the file pointer */
	fclose(fp);

	switch (fourCC)
	{
		// Okay... looks like we're only deadling with RGBA formats (not RGB formats)
		//  => only set RGBA formats!
		// Furthermore, assume just UNORM, but could also be sRGB... who knows?!
		// TODO: Test if sRGB looks better than UNORM!
	case FOURCC_DXT1:
		format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; // TODO: maybe VK_FORMAT_BC1_RGBA_SRGB_BLOCK?
		break;
	case FOURCC_DXT3:
		format = VK_FORMAT_BC2_UNORM_BLOCK; // TODO: maybe VK_FORMAT_BC2_RGBA_SRGB_BLOCK? And are we sure about BC2? Could it be that it is BC3 (whatever BC3 is)?
		break;
	case FOURCC_DXT5:
		format = VK_FORMAT_BC3_UNORM_BLOCK; // TODO: maybe VK_FORMAT_BC3_RGBA_SRGB_BLOCK? And are we sure about BC3? Could it be that it is BC5 (whatever BC5 is)?
		break;
	default:
		delete[] buffer;
		VKL_EXIT_WITH_ERROR("Unable to determine the DDS file's format (seems to be neither DXT1, nor DXT3, nor DXT5)");
	}

	unsigned int blockSize = (format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK) ? 8 : 16;
	uint32_t size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
	assert(size == bufsize);

	auto host_coherent_buffer = vklCreateHostCoherentBufferWithBackingMemory(bufsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vklCopyDataIntoHostCoherentBuffer(host_coherent_buffer, buffer, bufsize);

	delete[] buffer;
#endif
	return host_coherent_buffer;
}

VkBuffer vklLoadDdsImageLevelIntoHostCoherentBuffer(const char* file, uint32_t level)
{
	return vklLoadDdsImageFaceLevelIntoHostCoherentBuffer(file, 0u, level);
}

VkBuffer vklLoadDdsImageIntoHostCoherentBuffer(const char* file)
{
	return vklLoadDdsImageLevelIntoHostCoherentBuffer(file, 0u);
}

glm::mat4 vklCreatePerspectiveProjectionMatrix(float field_of_view, float aspect_ratio, float near_plane_distance, float far_plane_distance)
{
	static const glm::mat4 sInverseRotateAroundXFrom_RH_Yup_to_RH_Ydown = std::invoke([]() {
		// We assume all the spaces up to and including view space to feature a Y-axis pointing upwards.
		// Screen space in Vulkan, however, has the +Y-axis pointing downwards, +X to the left, and +Z into the screen.
		// We are staying in right-handed (RH) coordinate systems throughout ALL the spaces.
		// 
		// Therefore, we are representing the coordinates from here on -- actually exactly BETWEEN View Space and
		// Clip Space -- in a coordinate system which is rotated 180 degrees around X, having Y point down, still RH.
		const glm::mat4 rotateAroundXFrom_RH_Yup_to_RH_Ydown = glm::mat4{
			 glm::vec4{ 1.f,  0.f,  0.f,  0.f},
			-glm::vec4{ 0.f,  1.f,  0.f,  0.f},
			-glm::vec4{ 0.f,  0.f,  1.f,  0.f},
			 glm::vec4{ 0.f,  0.f,  0.f,  1.f},
		};

		// ...in order to represent coordinates in that aforementioned space, we need the inverse, u know:
		return glm::inverse(rotateAroundXFrom_RH_Yup_to_RH_Ydown);
	});

	// Scaling factor for the x and y coordinates which depends on the 
	// field of view (and the aspect ratio... see matrix construction)
	auto xyScale = 1.0f / glm::tan(field_of_view / 2.f);
	auto F_N = far_plane_distance - near_plane_distance;
	auto zScale = far_plane_distance / F_N;

	glm::mat4 m(0.0f);
	m[0][0] = xyScale / aspect_ratio;
	m[1][1] = xyScale;
	m[2][2] = zScale;
	m[2][3] = 1.f; // Offset z...
	m[3][2] = -near_plane_distance * zScale; // ... by this amount

	return m * sInverseRotateAroundXFrom_RH_Yup_to_RH_Ydown;
}

std::string loadModelFromFile(const std::string& model_filename)
{
    std::string path = {};

	std::ifstream infile(model_filename);
	if (infile.good()) {
		path = model_filename;
		VKL_LOG("Loading 3D model file from path[" << path << "]...");
	}

	if (path.empty()) { // Fail if model file could not be found:
		VKL_EXIT_WITH_ERROR("Unable to load file[" << model_filename << "].");
	}

	std::ifstream ifs(path);
	std::string content(
		(std::istreambuf_iterator<char>(ifs)),
		(std::istreambuf_iterator<char>())
	);
	return content;
}

VklGeometryData vklLoadModelGeometry(const std::string& path_to_obj)
{
	tinyobj::attrib_t attributes;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warning;
	std::string error;
	std::istringstream sourceStream(loadModelFromFile(path_to_obj));
	if (!tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, &sourceStream)) {
		VKL_EXIT_WITH_ERROR("Failed attempt to load model in OBJ format from [" << path_to_obj << "]. Warning[" << warning << "], Error[" << error << "]");
	}

	VklGeometryData data;
	for (const tinyobj::shape_t& shape : shapes) {
        std::map<std::tuple<int, int, int>, uint32_t> uniqueVertices;
        
		for (const auto& indices : shape.mesh.indices) {
			glm::vec3 pos = glm::vec3(
				attributes.vertices[3 * indices.vertex_index], 
				attributes.vertices[3 * indices.vertex_index + 1], 
				attributes.vertices[3 * indices.vertex_index + 2]
			);
			glm::vec2 uv = glm::vec2(
				attributes.texcoords[2 * indices.texcoord_index], 
				1.0f - attributes.texcoords[2 * indices.texcoord_index + 1]
			);
			glm::vec3 normal = glm::vec3(
				attributes.normals[3 * indices.normal_index], 
				attributes.normals[3 * indices.normal_index + 1], 
				attributes.normals[3 * indices.normal_index + 2]
			);
            
            std::tuple<int,int,int> tuple = std::tuple<int,int,int>
                (indices.vertex_index, indices.normal_index, indices.texcoord_index);
            
            if (uniqueVertices.find(tuple) == uniqueVertices.end()) {
                uniqueVertices.insert({tuple, static_cast<uint32_t>(data.positions.size())});
                data.positions.push_back(pos);
                data.textureCoordinates.push_back(uv);
				data.normals.push_back(normal);
			}
			data.indices.push_back(uniqueVertices[tuple]);
		}
	}
	return data;
}
