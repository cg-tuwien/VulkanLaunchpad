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

// Always use GLI, since the manual implementation of DDS loading does not currently work.
#define USE_GLI

#include <slang.h>
#include <slang-com-ptr.h>

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

vk::Instance mInstance                                          = {};
vk::SurfaceKHR mSurface                                         = {};
vk::PhysicalDevice mPhysicalDevice                              = {};
vk::Device mDevice                                              = {};
#if VK_HEADER_VERSION >= 302
#define DISPATCH_LOADER_NAMESPACE vk::detail
#else 
#define DISPATCH_LOADER_NAMESPACE vk
#endif
DISPATCH_LOADER_NAMESPACE::DispatchLoaderStatic mDispatchLoader = {};
vk::Queue mQueue                                                = {};
VklSwapchainConfig mSwapchainConfig                             = {};
std::vector<std::vector<vk::ClearValue>> mClearValues;

#ifdef VKL_HAS_VMA
VmaAllocator mVmaAllocator                                      = {};
bool vklHasVmaAllocator()                                       { return VmaAllocator{} != mVmaAllocator; }
#endif

bool mFrameworkInitialized                                      = false;

DISPATCH_LOADER_NAMESPACE::DispatchLoaderDynamic mDynamicDispatch;
vk::ResultValueType<VULKAN_HPP_NAMESPACE::DebugUtilsMessengerEXT>::type mDebugUtilsMessenger;
std::vector<std::vector<vk::ImageView>> mSwapchainImageViews; //< Will be the length of #swapchain images
vk::PipelineStageFlags mSrcStages0;
vk::AccessFlags mSrcAccess0;
vk::PipelineStageFlags mDstStages0;
vk::AccessFlags mDstAccess0;
vk::UniqueRenderPass mRenderpass;
std::vector<vk::UniqueFramebuffer> mFramebuffers; //< Will be the length of #swapchain images
bool mHasDepthAttachments = false;

constexpr int CONCURRENT_FRAMES = 10;
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

GLFWwindow* mCallbackWindow = nullptr;
GLFWkeyfun mPreviousKeyCallback = nullptr;
int mKeyForShaderHotReloading = 0;
int mModKeysForShaderHotReloading = 0;
std::unordered_map<VkPipeline, std::tuple<VklGraphicsPipelineConfig, std::pair<std::string, std::string>, std::pair<std::string, std::string>, bool>> mUserKnownPipelines;
std::unordered_map<VkPipeline, vk::Pipeline> mPipelineSurrogates;
std::deque<std::tuple<int64_t, vk::Pipeline>> mPipelineGraveyard;

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
VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugUtilsMessengerCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
	vk::DebugUtilsMessageTypeFlagsEXT message_type,
	const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data);

std::string mSpaceForToString;

// Creates a shader module from the given Spir-V code, returns the created shader module and its create info.
std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> loadShaderFromSpirvAndCreateShaderModuleAndStageInfo(const uint32_t* spirv, size_t byteSize, const vk::ShaderStageFlagBits shaderStage, const char* entryPoint = "main")
{
	auto moduleCreateInfo = vk::ShaderModuleCreateInfo{}
		.setCodeSize(byteSize)
		.setPCode(spirv);

	auto shaderModule = mDevice.createShaderModule(moduleCreateInfo);

	auto shaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{}
		.setStage(shaderStage)
		.setModule(shaderModule)
		.setPName(entryPoint); // entry point

	return std::make_tuple(shaderModule, shaderStageCreateInfo);
}

std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> loadSlangShaderFromMemoryAndCreateShaderModulesAndStageInfos(const std::pair<std::string, std::string>& shaderCodeAndEntryPoint, const std::string& shaderName, const vk::ShaderStageFlagBits shaderStage)
{
    std::string_view shaderType = shaderStage == vk::ShaderStageFlagBits::eFragment ? "fragment" : "vertex";

    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    auto globalSlangSessionResult = slang::createGlobalSession(slangGlobalSession.writeRef());
    if (SLANG_FAILED(globalSlangSessionResult)) {
        VKL_EXIT_WITH_ERROR("Failed to create Slang global session.");
    }

    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = slangGlobalSession->findProfile("spirv_1_3");

    slang::CompilerOptionEntry compilerOptionEntryPointName;
    compilerOptionEntryPointName.name = slang::CompilerOptionName::VulkanUseEntryPointName;
    compilerOptionEntryPointName.value.kind = slang::CompilerOptionValueKind::Int;
    compilerOptionEntryPointName.value.intValue0 = 1;

    slang::CompilerOptionEntry compilerOptionOptimization;
    compilerOptionOptimization.name = slang::CompilerOptionName::Optimization;
    compilerOptionOptimization.value.kind = slang::CompilerOptionValueKind::Int;
    compilerOptionOptimization.value.intValue0 = 3;

    slang::CompilerOptionEntry compilerOptionMatrixLayout;
    compilerOptionMatrixLayout.name = slang::CompilerOptionName::MatrixLayoutColumn;
    compilerOptionMatrixLayout.value.kind = slang::CompilerOptionValueKind::Int;
    compilerOptionMatrixLayout.value.intValue0 = 1;

    std::array<slang::CompilerOptionEntry, 3> compilerOptions = {
        compilerOptionEntryPointName,
        compilerOptionOptimization,
        compilerOptionMatrixLayout
    };

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.compilerOptionEntries = compilerOptions.data();
    sessionDesc.compilerOptionEntryCount = compilerOptions.size();

    Slang::ComPtr<slang::ISession> session;
    auto slangSessionResult = slangGlobalSession->createSession(sessionDesc, session.writeRef());
    if (SLANG_FAILED(slangSessionResult)) {
        VKL_EXIT_WITH_ERROR("Failed to create Slang session.");
    }

    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slangModule = session->loadModuleFromSourceString(
            shaderName.c_str(),
            nullptr,
            shaderCodeAndEntryPoint.first.c_str(),
            diagnosticsBlob.writeRef()
        );
        if (diagnosticsBlob != nullptr) {
            std::cout << "\nERROR:   Failed to load shader[" << shaderName << "]"
                      << "\n        " << (const char*)diagnosticsBlob->getBufferPointer() << std::endl;
            return std::make_tuple(vk::ShaderModule{ VK_NULL_HANDLE }, vk::PipelineShaderStageCreateInfo{});
        }
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    slangModule->findEntryPointByName(shaderCodeAndEntryPoint.second.c_str(), entryPoint.writeRef());
    if (!entryPoint) {
        std::cout << "\nERROR:   Failed to load shader[" << shaderName << "]"
                  << "\n         Error getting entry point \"" << shaderCodeAndEntryPoint.second << "\""
                  << "\n         Make sure to provide a function  \"" << shaderCodeAndEntryPoint.second << "\" annotated with [shader(\"" << shaderType << "\")]" << std::endl;
        return std::make_tuple(vk::ShaderModule{ VK_NULL_HANDLE }, vk::PipelineShaderStageCreateInfo{});
    }

    std::array<slang::IComponentType*, 2> componentTypes = { slangModule, entryPoint };

    Slang::ComPtr<slang::IComponentType> program;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = session->createCompositeComponentType(
            componentTypes.data(),
            componentTypes.size(),
            program.writeRef(),
            diagnosticsBlob.writeRef()
        );

        if (diagnosticsBlob != nullptr) {
            std::cout << "\nERROR:   Failed to compose shader[" << shaderName << "] for \"" << shaderType << "\" stage"
                      << "\n        " << (const char*)diagnosticsBlob->getBufferPointer() << std::endl;
        }
        if (SLANG_FAILED(result)) {
            return std::make_tuple(vk::ShaderModule{ VK_NULL_HANDLE }, vk::PipelineShaderStageCreateInfo{});
        }
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = program->link(
            linkedProgram.writeRef(),
            diagnosticsBlob.writeRef()
        );

        if (diagnosticsBlob != nullptr) {
            std::cout << "\nERROR:   Failed to link shader[" << shaderName << "] for \"" << shaderType << "\" stage"
                      << "\n        " << (const char*)diagnosticsBlob->getBufferPointer() << std::endl;
        }
        if (SLANG_FAILED(result)) {
            return std::make_tuple(vk::ShaderModule{ VK_NULL_HANDLE }, vk::PipelineShaderStageCreateInfo{});
        }
    }

    Slang::ComPtr<slang::IBlob> spirvCode;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = linkedProgram->getEntryPointCode(
            0,
            0,
            spirvCode.writeRef(),
            diagnosticsBlob.writeRef()
        );

        if (diagnosticsBlob != nullptr) {
            std::cout << "\nERROR:   Failed to get entry point code in shader[" << shaderName << "] for \"" << shaderType << "\" stage"
                      << "\n        " << (const char*)diagnosticsBlob->getBufferPointer() << std::endl;
        }
        if (SLANG_FAILED(result)) {
            return std::make_tuple(vk::ShaderModule{ VK_NULL_HANDLE }, vk::PipelineShaderStageCreateInfo{});
        }
    }


    return loadShaderFromSpirvAndCreateShaderModuleAndStageInfo(
        static_cast<const uint32_t*>(spirvCode->getBufferPointer()),
        spirvCode->getBufferSize(),
        shaderStage,
        shaderCodeAndEntryPoint.second.c_str()
    );
}

std::string loadSlangShaderCodeFromFile(const std::string& shader_filename)
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

    return content;
}

vk::Pipeline createGraphicsPipelineInternal(const VklGraphicsPipelineConfig& config, bool loadShadersFromMemoryInstead)
{
    if (!loadShadersFromMemoryInstead && !vklFrameworkInitialized()) {
        VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
    }

    if (!config.vertexShaderPathAndEntrypoint.first || !config.vertexShaderPathAndEntrypoint.second || !config.fragmentShaderPathAndEntrypoint.first || !config.fragmentShaderPathAndEntrypoint.second) {
        std::cout << "\nERROR:   Unable to create graphics pipeline, no valid shader paths and entrypoints provided."
                  << "\n         vertexShaderPathAndEntrypoint and fragmentShaderPathAndEntrypoint must contain path and entrypoint." << std::endl;
        return VK_NULL_HANDLE;
    }

    auto vertexShaderPathAndEntryPoint = std::make_pair(std::string(config.vertexShaderPathAndEntrypoint.first), std::string(config.vertexShaderPathAndEntrypoint.second));
    auto fragmentShaderPathAndEntryPoint = std::make_pair(std::string(config.fragmentShaderPathAndEntrypoint.first), std::string(config.fragmentShaderPathAndEntrypoint.second));

    std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> vertTpl;
    std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> fragTpl;

    if (loadShadersFromMemoryInstead) {
        vertTpl = loadSlangShaderFromMemoryAndCreateShaderModulesAndStageInfos(vertexShaderPathAndEntryPoint, "vertex_shader_from_memory", vk::ShaderStageFlagBits::eVertex);
        fragTpl = loadSlangShaderFromMemoryAndCreateShaderModulesAndStageInfos(fragmentShaderPathAndEntryPoint, "fragment_shader_from_memory", vk::ShaderStageFlagBits::eFragment);
    } else {
        std::string vertexShaderCode = loadSlangShaderCodeFromFile(vertexShaderPathAndEntryPoint.first);
        std::string fragmentShaderCode = loadSlangShaderCodeFromFile(fragmentShaderPathAndEntryPoint.first);
        vertTpl = loadSlangShaderFromMemoryAndCreateShaderModulesAndStageInfos({vertexShaderCode, vertexShaderPathAndEntryPoint.second}, vertexShaderPathAndEntryPoint.first, vk::ShaderStageFlagBits::eVertex);
        fragTpl = loadSlangShaderFromMemoryAndCreateShaderModulesAndStageInfos({fragmentShaderCode, fragmentShaderPathAndEntryPoint.second}, fragmentShaderPathAndEntryPoint.first, vk::ShaderStageFlagBits::eFragment);
    }

    if (!std::get<vk::ShaderModule>(vertTpl) || !std::get<vk::ShaderModule>(fragTpl)) {
        if (std::get<vk::ShaderModule>(vertTpl)) {
            mDevice.destroyShaderModule(std::get<vk::ShaderModule>(vertTpl));
        }
        if (std::get<vk::ShaderModule>(fragTpl)) {
            mDevice.destroyShaderModule(std::get<vk::ShaderModule>(fragTpl));
        }

        return VK_NULL_HANDLE;
    }

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

	mPipelineLayouts[graphicsPipeline] = std::forward_as_tuple(std::move(descriptorSetLayout), std::move(pipelineLayout));
	return graphicsPipeline;
}

vk::Pipeline vklCreateGraphicsPipeline(const VklGraphicsPipelineConfig& config, bool loadShadersFromMemoryInstead)
{
	auto graphicsPipelineHandle = createGraphicsPipelineInternal(config, loadShadersFromMemoryInstead);
	if (VK_NULL_HANDLE == graphicsPipelineHandle) {
        VKL_EXIT_WITH_ERROR("Failed to create graphics pipeline. Check console output if there were any problems with shader compilation!");
	}
	// Store for hot reloading, but only those handles, which the user requested explicitly (hence the split of createGraphicsPipelineInternal and vklCreateGraphicsPipeline):
	mUserKnownPipelines[graphicsPipelineHandle] = std::make_tuple(config, std::make_pair(std::string(config.vertexShaderPathAndEntrypoint.first), std::string(config.vertexShaderPathAndEntrypoint.second)), std::make_pair(std::string(config.fragmentShaderPathAndEntrypoint.first), std::string(config.fragmentShaderPathAndEntrypoint.second)), loadShadersFromMemoryInstead);
	return graphicsPipelineHandle;
}

vk::Pipeline getGraphicsPipelineOrItsSurrogate(vk::Pipeline originalPipelineHandle)
{
	auto it = mPipelineSurrogates.find(originalPipelineHandle);
	if (it != mPipelineSurrogates.end()) {
		return it->second; // using the surrogate/updated pipeline
	}
	return originalPipelineHandle;
}

void destroyGraphicsPipelineInternal(vk::Pipeline pipeline)
{
	mDevice.destroy(pipeline);

	// Also remove it from the graveyard:
	mPipelineGraveyard.erase(std::remove_if(
			mPipelineGraveyard.begin(),
			mPipelineGraveyard.end(),
			[pipeline](const std::tuple<int64_t, vk::Pipeline>& element) {
				return std::get<1>(element) == pipeline; 
			}
		), mPipelineGraveyard.end());
	// ...and as a surrogate (i.e., "pointed to"):
	for(auto it = mPipelineSurrogates.begin(); it != mPipelineSurrogates.end();) {
		if (it->second == pipeline) {
			it = mPipelineSurrogates.erase(it);
		}
		else {
			++it;
		}
	}
	// but NOT from known pipelines!
}

void vklDestroyGraphicsPipeline(vk::Pipeline pipeline)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}

	// Destroy the latest surrogate:
	destroyGraphicsPipelineInternal(getGraphicsPipelineOrItsSurrogate(pipeline));

	// Remove the ORIGINAL pipeline handle from known pipelines:
	auto it = mUserKnownPipelines.find(pipeline);
	if (it != mUserKnownPipelines.end()) {
		mUserKnownPipelines.erase(it);
	}
	// ...and potentially also from surrogate list (if it is the "points from" entry):
	for(auto it = mPipelineSurrogates.begin(); it != mPipelineSurrogates.end();) {
		if (it->first == pipeline) {
			it = mPipelineSurrogates.erase(it);
		}
		else {
			++it;
		}
	}
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

vk::DeviceMemory vklAllocateMemoryForGivenRequirements(vk::DeviceSize bufferSize, vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags)
{
    const auto memoryAllocInfo = vklCreateMemoryAllocateInfo(bufferSize, memoryRequirements, memoryPropertyFlags);

    // Allocate:
    vk::DeviceMemory memory;
    auto returnCode = mDevice.allocateMemory(&memoryAllocInfo, nullptr, &memory);
    if (returnCode == vk::Result::eSuccess) {
        return memory;
    }
    VKL_EXIT_WITH_ERROR(std::string("Error allocating memory of size [") + std::to_string(bufferSize) + "] and requirements[" + std::to_string(memoryRequirements.alignment) + ", " + std::to_string(memoryRequirements.memoryTypeBits) + ", " + std::to_string(memoryRequirements.size) + "]\n    Error Code: " + vk::to_string(returnCode));
}

vk::UniqueDeviceMemory vklAllocateUniqueMemoryForGivenRequirements(vk::DeviceSize bufferSize, vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags) {
  const auto memoryAllocInfo = vklCreateMemoryAllocateInfo(bufferSize, memoryRequirements, memoryPropertyFlags);
  auto allocatedMemory = mDevice.allocateMemoryUnique(memoryAllocInfo, nullptr, mDispatchLoader);
  return allocatedMemory;
}

vk::Buffer vklCreateHostCoherentBufferWithBackingMemory(vk::DeviceSize buffer_size, vk::BufferUsageFlags buffer_usage)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Describe a new buffer:
	auto createInfo = vk::BufferCreateInfo{}
		.setSize(buffer_size)
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
    auto memory = vklAllocateUniqueMemoryForGivenRequirements(buffer_size, mDevice.getBufferMemoryRequirements(buffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
	// Bind the buffer handle to the memory:
	mDevice.bindBufferMemory(buffer, memory.get(), 0);

	// Remember the assignment:
	mHostCoherentBuffersWithBackingMemory[buffer] = std::move(memory);

	return buffer;
}

vk::Buffer vklCreateDeviceLocalBufferWithBackingMemory(vk::DeviceSize buffer_size, vk::BufferUsageFlags buffer_usage)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	// Describe a new buffer:
	auto createInfo = vk::BufferCreateInfo{}
		.setSize(buffer_size)
		.setUsage(buffer_usage);

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
	auto memory = vklAllocateUniqueMemoryForGivenRequirements(buffer_size, mDevice.getBufferMemoryRequirements(buffer), vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Bind the buffer handle to the memory:
	mDevice.bindBufferMemory(buffer, memory.get(), 0);

	// Remember the assignment:
	mDeviceLocalBuffersWithBackingMemory[buffer] = std::move(memory);

	return buffer;
}

void vklDestroyHostCoherentBufferAndItsBackingMemory(vk::Buffer buffer)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	if (vk::Buffer{} == buffer) {
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
		mDevice.destroy(buffer);
	}
}

void vklDestroyDeviceLocalBufferAndItsBackingMemory(vk::Buffer buffer)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	if (vk::Buffer{} == buffer) {
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
		mDevice.destroy(buffer);
	}
}

void vklCopyDataIntoHostCoherentBuffer(vk::Buffer buffer, const void* data_pointer, size_t data_size_in_bytes)
{
	vklCopyDataIntoHostCoherentBuffer(buffer, 0, data_pointer, data_size_in_bytes);
}

void vklCopyDataIntoHostCoherentBuffer(vk::Buffer buffer, size_t buffer_offset_in_bytes, const void* data_pointer, size_t data_size_in_bytes)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	if (vk::Buffer{} == buffer) {
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
vk::Buffer vklCreateHostCoherentBufferAndUploadData(const void* data, size_t size, vk::BufferUsageFlags usageFlags) {
    vk::Buffer result;
    result = vklCreateHostCoherentBufferWithBackingMemory(size, vk::BufferUsageFlagBits::eTransferDst | usageFlags);
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

void vklBindDescriptorSetToPipeline(vk::DescriptorSet descriptor_set, vk::Pipeline pipeline)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	if (mSingleUseCommandBuffers.empty()) {
		VKL_EXIT_WITH_ERROR("There are no command buffers to record commands into. Have you called vklStartRecordingCommands() beforehand?");
	}
	auto& cb = mSingleUseCommandBuffers.back().get();

	pipeline = getGraphicsPipelineOrItsSurrogate(pipeline);

	auto searchPl = mPipelineLayouts.find(pipeline);
	if (mPipelineLayouts.end() == searchPl) {
		VKL_EXIT_WITH_ERROR("Couldn't find the VkPipeline passed to vklBindDescriptorSetToPipeline. Is it a valid handle and has it been created with vklCreateGraphicsPipeline(...)?");
	}

	auto pipeLayout = std::get<vk::UniquePipelineLayout>(searchPl->second).get();

	cb.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics, pipeLayout,
		0u, 1u, &descriptor_set, // <--- Bind the actual descriptors to the pipeline
		0u, nullptr
	);
}

vk::PipelineLayout vklGetLayoutForPipeline(vk::Pipeline pipeline)
{
	pipeline = getGraphicsPipelineOrItsSurrogate(pipeline);

	auto searchPl = mPipelineLayouts.find(pipeline);
	if (mPipelineLayouts.end() == searchPl) {
		VKL_EXIT_WITH_ERROR("Couldn't find the VkPipeline passed to vklBindDescriptorSetToPipeline. Is it a valid handle and has it been created with vklCreateGraphicsPipeline(...)?");
	}
	return std::get<vk::UniquePipelineLayout>(searchPl->second).get();
}

bool vklInitFramework(vk::Instance vk_instance, vk::SurfaceKHR vk_surface, vk::PhysicalDevice vk_physical_device, vk::Device vk_device, vk::Queue vk_queue, const VklSwapchainConfig& swapchain_config)
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
			if (vk::Format{} == imageDetails[j].imageFormat) {
				VKL_EXIT_WITH_ERROR("Invalid VkFormat passed to vklInitFramework through VklSwapchainConfig::swapchainImages[" + std::to_string(i) + "]::imageDetails[" + std::to_string(j) + "]::imageFormat");
			}
			if (vk::ImageUsageFlags{} == imageDetails[j].imageUsage) {
				VKL_EXIT_WITH_ERROR("Invalid VkImageUsageFlags passed to vklInitFramework through VklSwapchainConfig::swapchainImages[" + std::to_string(i) + "]::imageDetails[" + std::to_string(j) + "]::imageUsage");
			}
		}
	}

	// Switch to Vulkan-Hpp (can't stand the C interface):
	mInstance = vk_instance;
	mSurface = vk_surface;
	mPhysicalDevice = vk_physical_device;
	mDevice = vk_device;
	mDispatchLoader = DISPATCH_LOADER_NAMESPACE::DispatchLoaderStatic();
	mQueue = vk_queue;
	mSwapchainConfig = swapchain_config;

	// Create a DYNAMIC DISPATCH LOADER:
	mDynamicDispatch = DISPATCH_LOADER_NAMESPACE::DispatchLoaderDynamic{ mInstance, vkGetInstanceProcAddr };

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
                                    + vk::to_string(attachments_i[j].imageFormat) + ", while element[0, " + std::to_string(j) + "] is set to " + vk::to_string(attachments_0[j].imageFormat));
			}
			if (attachments_i[j].imageUsage != attachments_0[j].imageUsage) {
				VKL_EXIT_WITH_ERROR("Corresponding VklSwapchainImageDetails::imageUsage entries must be set to the same values! However, element[" + std::to_string(i) + ", " + std::to_string(j) + "] is set to "
                                    + vk::to_string(attachments_i[j].imageUsage) + ", while element[0, " + std::to_string(j) + "] is set to " + vk::to_string(attachments_0[j].imageUsage));
			}

			// Create the views:
			if (vk::ImageUsageFlagBits::eDepthStencilAttachment & attachments_i[j].imageUsage) {
				// Create a view for a depth buffer:
				mSwapchainImageViews[i][j] = mDevice.createImageView(vk::ImageViewCreateInfo{
					{}, vk::Image{attachments_i[j].imageHandle },
					vk::ImageViewType::e2D, attachments_i[j].imageFormat,
					vk::ComponentMapping{},
					vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u }
				});
			}
			else {
				// Create a view for a color buffer:
				mSwapchainImageViews[i][j] = mDevice.createImageView(vk::ImageViewCreateInfo{
					{}, vk::Image{ attachments_i[j].imageHandle },
					vk::ImageViewType::e2D, attachments_i[j].imageFormat,
					vk::ComponentMapping{},
					vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u }
				});
			}
			
			// Gather information for the renderpass already:
			if (0 == i) {
				auto curAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());

				attachmentDescriptions.emplace_back(vk::AttachmentDescription{}
					.setFormat(attachments_i[j].imageFormat)
					.setLoadOp(vk::AttachmentLoadOp::eClear)		// What do do with the image when the renderpass starts? => Make sure that we have cleared the content of previous frames!
					.setStoreOp( // What to do with the image when the renderpass has finished? => We don't need the depth buffer for anything afterwards.
						vk::ImageUsageFlagBits::eDepthStencilAttachment & attachments_i[j].imageUsage
						? vk::AttachmentStoreOp::eDontCare
						: vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eUndefined)	// When the renderpass starts, in which layout will the image be? => We don't care since we're clearing it.
					.setFinalLayout( // When the renderpass finishes, in which layout shall the image be transfered? => The image shall be presented directly afterwards. 
						vk::ImageUsageFlagBits::eDepthStencilAttachment & attachments_i[j].imageUsage
						? vk::ImageLayout::eDepthStencilAttachmentOptimal // When the renderpass finishes, in which layout shall the image be transferred? => It will be in eDepthStencilAttachmentOptimal layout anyways.
						: vk::ImageLayout::ePresentSrcKHR)
					.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
					.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
				);
				
				if (vk::ImageUsageFlagBits::eDepthStencilAttachment & attachments_i[j].imageUsage) {
					depthAttachmentsInSubpass0.emplace_back(curAttachmentIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal); // Describes the index (w.r.t. attachmentDescriptions) and the desired layout of the depth attachment for subpass 0
				}
				else {
					colorAttachmentsInSubpass0.emplace_back(curAttachmentIndex, vk::ImageLayout::eColorAttachmentOptimal); // Describes the index (w.r.t. attachmentDescriptions) and the desired layout of the color attachment for subpass 0
				}
			}

			currentClearValues.emplace_back(attachments_i[j].clearValue);
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

	mBasicPipeline = createGraphicsPipelineInternal(VklGraphicsPipelineConfig{
        std::make_pair(
            "struct VSInput {\n"
            "   float3 position : POSITION;\n"
            "};\n"
            "\n"
            "struct VSOutput {\n"
            "   float4 position : SV_Position;\n"
            "};\n"
            "\n"
            "[shader(\"vertex\")]\n"
            "VSOutput vertexMain(VSInput input) {\n"
            "   VSOutput output;\n"
            "   output.position = float4(input.position.x, -input.position.y, input.position.z, 1.0);\n"
            "   return output;\n"
            "}\n",
            "vertexMain"
        ),
        std::make_pair(
            "struct FSOutput {\n"
            "   float4 color : SV_Target;\n"
            "};\n"
            "\n"
            "[shader(\"fragment\")]\n"
            "FSOutput fragmentMain() {\n"
            "   FSOutput output;\n"
            "   output.color = float4(1.0, 0.0, 0.0, 1.0);\n"
            "   return output;\n"
            "}\n",
            "fragmentMain"
        ),
		// Further config parameters:
		{
			vk::VertexInputBindingDescription { 0, sizeof(glm::vec3), vk::VertexInputRate::eVertex }
		},
		{
			vk::VertexInputAttributeDescription { 0, 0, vk::Format::eR32G32B32Sfloat, 0u }
		},
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		{ /* no descriptors */ }
	}, /* load shaders from memory: */ true);

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

// Delete those pipelines which are no longer used due having been replaced after hot reloading
void destroyOutdatedPipelines() 
{
	while (!mPipelineGraveyard.empty() && std::get<0>(*mPipelineGraveyard.begin()) < mFrameId) {
		destroyGraphicsPipelineInternal(std::get<1>(*mPipelineGraveyard.begin()));
	}
}

double vklWaitForNextSwapchainImage()
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}

	destroyOutdatedPipelines();

	// Advance the frame ID:
	++mFrameId;
	mFrameInFlightIndex = mFrameId % CONCURRENT_FRAMES;

	// Just out of curiosity, measure the wait time:
	auto t0 = glfwGetTime();

	// Wait for the fence of the current image before reusing the same image available semaphore (as we have used #CONCURRENT_FRAMES in the past)
	vk::Result returnCode = mDevice.waitForFences(1u, &mSyncHostWithDeviceFence[mFrameInFlightIndex].get(), VK_TRUE, std::numeric_limits<uint64_t>::max()); // Wait up to forever
	VKL_CHECK_VULKAN_ERROR(returnCode);

	returnCode = mDevice.resetFences(1u, &mSyncHostWithDeviceFence[mFrameInFlightIndex].get());
	VKL_CHECK_VULKAN_ERROR(returnCode);

	// Keep house with the in-flight images:
	for (auto& mapping : mImagesInFlightFenceIndices) { // However, we don't know which index this fence had been mapped to => we have to search
		if (mFrameInFlightIndex == mapping) {
			mapping = -1;
			break;
		}
	}

	// Get the next image from the swap chain:
	mCurrentSwapChainImageIndex = mDevice.acquireNextImageKHR(mSwapchainConfig.swapchainHandle, std::numeric_limits<uint64_t>::max(), mImageAvailableSemaphores[mFrameInFlightIndex].get(), nullptr).value;
	// Safety-check on the returned image index:
	if (mImagesInFlightFenceIndices[mCurrentSwapChainImageIndex] >= 0) {
		// it is set => must perform an extra wait
		returnCode = mDevice.waitForFences(1u, &mSyncHostWithDeviceFence[mImagesInFlightFenceIndices[mCurrentSwapChainImageIndex]].get(), VK_TRUE, std::numeric_limits<uint64_t>::max()); // Wait up to forever
		VKL_CHECK_VULKAN_ERROR(returnCode);
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
	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1u)
		.setPWaitSemaphores(&mRenderFinishedSemaphores[mFrameInFlightIndex].get())
		.setSwapchainCount(1u)
		.setPSwapchains(&mSwapchainConfig.swapchainHandle)
		.setPImageIndices(&mCurrentSwapChainImageIndex);
	
	vk::Result returnCode = mQueue.presentKHR(presentInfo);
	VKL_CHECK_VULKAN_ERROR(returnCode);

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
vk::Framebuffer vklGetFramebuffer(uint32_t i)
{
	if (i >= mFramebuffers.size()) {
		VKL_EXIT_WITH_ERROR("The given index[" + std::to_string(i) + "] is larger than the number of available framebuffers[" + std::to_string(mFramebuffers.size()) + "]");
	}
	return mFramebuffers[i].get();
}
vk::Framebuffer vklGetCurrentFramebuffer()
{
	return vklGetFramebuffer(vklGetCurrentSwapChainImageIndex());
}
vk::RenderPass vklGetRenderpass()
{
	return mRenderpass.get();
}
vk::CommandBuffer vklGetCurrentCommandBuffer()
{
	if(mSingleUseCommandBuffers.empty()) {
		VKL_EXIT_WITH_ERROR("There are no command buffers. Have you called vklStartRecordingCommands beforehand?");
	}
	const auto& cb = mSingleUseCommandBuffers.back().get();
	return cb;
}

vk::Pipeline vklGetBasicPipeline()
{
    return mBasicPipeline;
}

vk::Device vklGetDevice()
{
  return mDevice;
}

vk::Image vklCreateDeviceLocalImageWithBackingMemory(vk::PhysicalDevice physical_device, vk::Device device, uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage_flags, uint32_t array_layers, vk::ImageCreateFlags flags)
{
	auto createInfo = vk::ImageCreateInfo{}
		.setFlags(flags)
		.setImageType(vk::ImageType::e2D)
		.setExtent({ width, height, 1u })
		.setMipLevels(static_cast<uint32_t>(1 + std::floor(std::log2(std::max(width, height)))))
		.setArrayLayers(array_layers)
		.setFormat(format)
		.setTiling(vk::ImageTiling::eOptimal)			// We just create all images in optimal tiling layout
		.setInitialLayout(vk::ImageLayout::eUndefined)	// Initially, the layout is undefined
		.setUsage(usage_flags)
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

	auto image = device.createImage(createInfo);

	auto memoryRequirements = device.getImageMemoryRequirements(image);

	auto memoryAllocInfo = vk::MemoryAllocateInfo{}
		.setAllocationSize(memoryRequirements.size)
		.setMemoryTypeIndex([&]() {
			// Get memory types supported by the physical device:
			auto memoryProperties = physical_device.getMemoryProperties();

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

	auto memory = device.allocateMemoryUnique(memoryAllocInfo, nullptr, mDispatchLoader);

	device.bindImageMemory(image, memory.get(), 0);

	// Remember the assignment:
	mImagesWithBackingMemory[static_cast<VkImage>(image)] = std::move(memory);

	return image;
}

vk::Image vklCreateDeviceLocalImageWithBackingMemory(vk::PhysicalDevice physical_device, vk::Device device, uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage_flags)
{
	return vklCreateDeviceLocalImageWithBackingMemory(physical_device, device, width, height, format, usage_flags, /* one layer: */ 1u, /* no flags: */{});
}

vk::Image vklCreateDeviceLocalImageWithBackingMemory(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage_flags)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	return vklCreateDeviceLocalImageWithBackingMemory(mPhysicalDevice, mDevice, width, height, format, usage_flags);
}

vk::Image vklCreateDeviceLocalImageWithBackingMemory(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage_flags, uint32_t array_layers, vk::ImageCreateFlags flags)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to invoke vklInitFramework beforehand!");
	}
	return vklCreateDeviceLocalImageWithBackingMemory(mPhysicalDevice, mDevice, width, height, format, usage_flags, array_layers, flags);
}

void vklDestroyDeviceLocalImageAndItsBackingMemory(vk::Image image)
{
	if (!vklFrameworkInitialized()) {
		VKL_EXIT_WITH_ERROR("Framework not initialized. Ensure to not invoke vklDestroyFramework beforehand!");
	}
	if (vk::Image{} == image) {
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
		mDevice.destroy(image);
	}
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugUtilsMessengerCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
	vk::DebugUtilsMessageTypeFlagsEXT message_type,
	const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data)
{
	if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		std::cout << "\nERROR:   messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		std::cout << "\nWARNING: messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
	{
		std::cout << "\nINFO:    messageIdNumber[" << callback_data->messageIdNumber << "], messageIdName[" << callback_data->pMessageIdName << "], message[" << callback_data->pMessage << "]" << std::endl;
	}
	else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
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
	vk::Format vkFormat;

	switch (gliFormat) {
		// See "Khronos Data Format Specification": https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#S3TC
		// And Vulkan specification: https://www.khronos.org/registry/vulkan/specs/1.2-khr-extensions/html/chap42.html#appendix-compressedtex-bc
	case gli::format::FORMAT_RGB_DXT1_UNORM_BLOCK8:
	    vkFormat = vk::Format::eBc1RgbSrgbBlock;
		break;
	case gli::format::FORMAT_RGB_DXT1_SRGB_BLOCK8:
	    vkFormat = vk::Format::eBc1RgbSrgbBlock;
		break;
	case gli::format::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
	    vkFormat = vk::Format::eBc1RgbaSrgbBlock;
		break;
	case gli::format::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
	    vkFormat = vk::Format::eBc1RgbaSrgbBlock;
		break;
	case gli::format::FORMAT_RGBA_DXT3_UNORM_BLOCK16:
	    vkFormat = vk::Format::eBc2SrgbBlock;
		break;
	case gli::format::FORMAT_RGBA_DXT3_SRGB_BLOCK16:
	    vkFormat = vk::Format::eBc2SrgbBlock;
		break;
	case gli::format::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
	    vkFormat = vk::Format::eBc3SrgbBlock;
		break;
	case gli::format::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
	    vkFormat = vk::Format::eBc3SrgbBlock;
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
		info.imageFormat= vk::Format::eBc1RgbaUnormBlock; // TODO: maybe VK_FORMAT_BC1_RGBA_SRGB_BLOCK?
		break;
	case FOURCC_DXT3:
		info.imageFormat = vk::Format::eBc2UnormBlock; // TODO: maybe VK_FORMAT_BC2_RGBA_SRGB_BLOCK? And are we sure about BC2? Could it be that it is BC3 (whatever BC3 is)?
		break;
	case FOURCC_DXT5:
		info.imageFormat = vk::Format::eBc3UnormBlock; // TODO: maybe VK_FORMAT_BC3_RGBA_SRGB_BLOCK? And are we sure about BC3? Could it be that it is BC5 (whatever BC5 is)?
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

vk::Buffer vklLoadDdsImageFaceLevelIntoHostCoherentBuffer(const char* file, uint32_t face, uint32_t level)
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
	auto host_coherent_buffer = vklCreateHostCoherentBufferWithBackingMemory(bufsize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
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
	vk::Format format;

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
		format = vk::Format::eBc1RgbaUnormBlock; // TODO: maybe VK_FORMAT_BC1_RGBA_SRGB_BLOCK?
		break;
	case FOURCC_DXT3:
		format = vk::Format::eBc2UnormBlock; // TODO: maybe VK_FORMAT_BC2_RGBA_SRGB_BLOCK? And are we sure about BC2? Could it be that it is BC3 (whatever BC3 is)?
		break;
	case FOURCC_DXT5:
		format = vk::Format::eBc3UnormBlock; // TODO: maybe VK_FORMAT_BC3_RGBA_SRGB_BLOCK? And are we sure about BC3? Could it be that it is BC5 (whatever BC5 is)?
		break;
	default:
		delete[] buffer;
		VKL_EXIT_WITH_ERROR("Unable to determine the DDS file's format (seems to be neither DXT1, nor DXT3, nor DXT5)");
	}

	unsigned int blockSize = (format == vk::Format::eBc1RgbaUnormBlock) ? 8 : 16;
	uint32_t size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
	assert(size == bufsize);

	auto host_coherent_buffer = vklCreateHostCoherentBufferWithBackingMemory(bufsize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
	vklCopyDataIntoHostCoherentBuffer(host_coherent_buffer, buffer, bufsize);

	delete[] buffer;
#endif
	return host_coherent_buffer;
}

vk::Buffer vklLoadDdsImageLevelIntoHostCoherentBuffer(const char* file, uint32_t level)
{
	return vklLoadDdsImageFaceLevelIntoHostCoherentBuffer(file, 0u, level);
}

vk::Buffer vklLoadDdsImageIntoHostCoherentBuffer(const char* file)
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

void vklHotReloadPipelines()
{
	VKL_LOG("About to hot-reload " << mUserKnownPipelines.size() << " known graphics pipelines...");
	for(auto it = mUserKnownPipelines.begin(); it != mUserKnownPipelines.end(); it++) {
		auto originalHandle = it->first;
		std::get<0>(it->second).vertexShaderPathAndEntrypoint.first         = std::get<1>(it->second).first.c_str();
		std::get<0>(it->second).vertexShaderPathAndEntrypoint.second        = std::get<1>(it->second).second.c_str();
		std::get<0>(it->second).fragmentShaderPathAndEntrypoint.first      = std::get<2>(it->second).first.c_str();
		std::get<0>(it->second).fragmentShaderPathAndEntrypoint.second      = std::get<2>(it->second).second.c_str();
		auto newHandle = createGraphicsPipelineInternal(std::get<0>(it->second), std::get<3>(it->second));
		if (VK_NULL_HANDLE == newHandle) {
			continue;
		}

		// We're going to destroy one outdated pipeline in any case (regardless the mapping):
		auto destroyHandle = getGraphicsPipelineOrItsSurrogate(originalHandle);
		mPipelineGraveyard.push_back(std::make_tuple(mFrameId + CONCURRENT_FRAMES, destroyHandle));

		// And we have a new surrogate for the original handle:
		mPipelineSurrogates[originalHandle] = newHandle;
	}
}

void pipelineHotReloadingCallback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE && key == mKeyForShaderHotReloading && mods == mModKeysForShaderHotReloading) {
		vklHotReloadPipelines();
	}
	if (nullptr != mPreviousKeyCallback) {
		mPreviousKeyCallback(mCallbackWindow, key, scancode, action, mods);
	}
}

void vklEnablePipelineHotReloading(GLFWwindow* glfw_window, int glfw_key, int glfw_modifier_keys)
{
	mCallbackWindow = glfw_window;
	mKeyForShaderHotReloading = glfw_key;
	mModKeysForShaderHotReloading = glfw_modifier_keys;
	auto previousCallback = glfwSetKeyCallback(glfw_window, pipelineHotReloadingCallback);
	if (previousCallback != pipelineHotReloadingCallback) {
		mPreviousKeyCallback = previousCallback;
	}
}

void vklCmdBindPipeline(vk::CommandBuffer commandBuffer, vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline)
{
	pipeline = getGraphicsPipelineOrItsSurrogate(pipeline);
    commandBuffer.bindPipeline(pipelineBindPoint, pipeline);
}
