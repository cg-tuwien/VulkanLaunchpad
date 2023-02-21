/*
 * Copyright (c) 2022 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 * Created by Johannes Unterguggenberger (junt@cg.tuwien.ac.at, https://johannesugb.github.io).
 */
#pragma once

#include <iostream>
#include <memory>
#include <cstring>
#include <vector>
#include <sstream>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#if __has_include(<vma/vk_mem_alloc.h>)
#define VKL_HAS_VMA
#include <vma/vk_mem_alloc.h>
#endif

// Returns a string describing the given VkResult value
extern const char *to_string(VkResult result);

#ifdef _WIN32
#define VKL_PATH_SEPARATOR '\\'
#else
#define VKL_PATH_SEPARATOR '/'
#endif

#define VKL_FILENAME (strrchr(__FILE__, VKL_PATH_SEPARATOR) ? strrchr(__FILE__, VKL_PATH_SEPARATOR) + 1 : __FILE__)

#define VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM " (in " << VKL_FILENAME << " at line #" << __LINE__ << ")"

#define VKL_LOG(log)             do { std::cout << "LOG:     " << log << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl; } while(false)

#define VKL_WARNING(log)         do { std::cout << "WARNING: " << log << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl; } while(false)

#define VKL_EXIT_WITH_ERROR(err) do { std::cout << "ERROR:   " << err << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl; glfwTerminate(); std::stringstream ss; ss << err; throw std::runtime_error(ss.str()); } while(false)

// Evaluates a VkResult and displays its status:
#define VKL_CHECK_VULKAN_RESULT(result) do { if ((result) < VK_SUCCESS) { std::cout << "ERROR:   Vulkan operation was not successful with error code " << to_string(result) << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl; } else { std::cout << "CHECK:   Vulkan operation returned status code: " << to_string(result) << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << "\n";  } } while(false)

// Evaluates a VkResult and displays its status only if it represents an error:
#define VKL_CHECK_VULKAN_ERROR(result)  do { if ((result) < VK_SUCCESS) { std::cout << "ERROR:   Vulkan operation was not successful with error code " << to_string(result) << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl; } } while(false)

// Evaluates a VkResult and issues a return statement if it represents an error:
#define VKL_RETURN_ON_ERROR(result)     do { if ((result) < VK_SUCCESS) { return; } } while(false)

/*!
 *	A struct containing details about one specific image that is used in a swap chain
 */
struct VklSwapchainImageDetails {
    /*! The image's handle: */
    VkImage imageHandle = VK_NULL_HANDLE;

    /*! The format of the image: */
    VkFormat imageFormat;

    /*! The usage of the image: */
    VkImageUsageFlags imageUsage;

    /*! The value that this image shall be cleared to at the beginning of a new frame: */
    VkClearValue clearValue;
};
/*!
 *	A struct describing the swap chain config in terms of used images.
 *	It can be perfectly valid to have only the colorImageDetails set.
 *
 *	If, only the color buffers is set, you must ensure that the depth buffer's
 *	imageHandle is set to VK_NULL_HANDLE, otherwise it will be interpreted as
 *	if there was a depth buffer specified.
 */
struct VklSwapchainFramebufferComposition {
    /*!
     *	Details about the color attachment image of this framebuffer composition.
     *	The color image must always be set. That means, its imageHandle must
     *	be a valid image handle.
     */
    VklSwapchainImageDetails colorAttachmentImageDetails = {};

    /*!
     *	Details about an optional depth attachment of this framebuffer composition.
     *	Specifying the depth image is optional. If its imageHandle is not set
     *	to a value different to VK_NULL_HANDLE, it is assumed that no depth
     *	image shall be used.
     */
    VklSwapchainImageDetails depthAttachmentImageDetails = {};
};

/*!
 *	A struct describing the swapchain config in terms of used images
 */
struct VklSwapchainConfig {
    /*! The handle of the already created swapchain: */
    VkSwapchainKHR swapchainHandle = VK_NULL_HANDLE;

    /*! The resolution of each swap chain image (they all must match!): */
    VkExtent2D imageExtent;

    /*! Provide one entry per swapchain image composition (can be one or multiple images): */
    std::vector<VklSwapchainFramebufferComposition> swapchainImages;
};

/*!
 *	A struct containing config parameters for the creation of a graphics pipeline
 */
struct VklGraphicsPipelineConfig {
    /*! The path to the vertex shader, which can be provided relative to the "assets/shader/" directory.
     *	That means that it will be tried to first load from the given value prepended with "assets/shader/".
     *	Only if that fails, it will be tried to load from the given value directly.
     */
    const char *vertexShaderPath = nullptr;

    /*! The path to the fragment shader, which can be provided relative to the "assets/shader/" directory.
     *	That means that it will be tried to first load from the given value prepended with "assets/shader/".
     *	Only if that fails, it will be tried to load from the given value directly.
     */
    const char *fragmentShaderPath = nullptr;

    /*!
     *	One description per buffer that is when rendering with a graphics pipeline.
     *	There are different approaches for this, most commonly:
     *	a) Either one buffer is bound which contains INTERLEAVED data, which means
     *	   that each buffer element contains, e.g., positions, normals, texture coordinates
     *	   after each other in that order => then only one description is required for this vector.
     *	b) The data (e.g., positions, normals, texture coordinates) are contained within
     *	   multiple different buffers and must be read from multiple different buffers. In
     *	   such cases, one description is required for every buffer that contains relevant data.
     *
     *	Hint: Set the .binding member of each element to steadily increasing numbers, starting with 0.
     */
    std::vector<VkVertexInputBindingDescription> vertexInputBuffers;

    /*!
     *	One description per input attribute as it is specified in vertex shaders.
     *
     *	Each input attribute description tells at which LOCATION the data can be read from in
     *	vertex shaders by setting its .location member!
     *
     *	Use the .binding member to establish the link from the attribute description to the
     *	input buffer, s.t., the .binding member of VkVertexInputBindingDescription and the
     *	.binding member of VkVertexInputAttributeDescription match!
     */
    std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions;

    /*! Sets the mode that is used for drawing polygons */
    VkPolygonMode polygonDrawMode;

    /*! Sets which triangles should be culled during rendering */
    VkCullModeFlags triangleCullingMode;

    /*!
     *	This vector describes the layout of resources that are bound to shaders.
     *	I.e., for each resource that is used in shaders, there must be one entry in this
     *	vector, specifying the correct binding-ID for the resource.
     *
     *	E.g., if there will be a uniform buffer bound to (location = 5), an entry must
     *	be created with the following properties: .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
     *	.binding        = 5
     */
    std::vector<VkDescriptorSetLayoutBinding> descriptorLayout;

    /*! If set to true, the pipeline will be configured to have blending enabled, 
     *  where its blend factors are set as follows: 
     *    srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA
     *    dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
     */
    bool enableAlphaBlending = false;
};

/*!
 *  This struct contains all data for a geometry object to be saved on the CPU-side and sent to the GPU.
 */
struct VklGeometryData {
    //! A vector of vertex positions, required.
    std::vector<glm::vec3> positions;

    //! A vector of vertex indices, required.
    /*! In this list each triplet of three indices define a triangle. I.e. it is not compressed. */
    std::vector<uint32_t> indices;

    //! A vector of vertex normals, optional.
    /*! If you do not desire to have any vertex normals, leave this vector empty. */
    std::vector<glm::vec3> normals;

    //! A vector of vertex texture coordinates, optional.
    /*! If you do not desire to have any texture coordinates for your vertices, leave this vector empty. */
    std::vector<glm::vec2> textureCoordinates;
};

/* --------------------------------------------- */
// Framework functions
/* --------------------------------------------- */

// An array of Vulkan instance extensions required by this framework
extern const char *vklRequiredInstanceExtensions[];

/*!
 *  This function returns an array of names of Vulkan instance extensions required
 *  by the framework. The number of elements will be returned via the outCount parameter.
 *  
 *  @param      out_count   Out-param. The variable pointed to will contain the number of required extensions.
 * 
 *  @returns    Returns the Vulkan instance extensions required by the Vulkan Launchpad Library as array of const char* elements.
 */
const char **vklGetRequiredInstanceExtensions(uint32_t *out_count);

/*!
 *  Initializes the framework
 */
bool vklInitFramework(VkInstance vk_instance, VkSurfaceKHR vk_surface, VkPhysicalDevice vk_physical_device,
                      VkDevice vk_device, VkQueue vk_queue, const VklSwapchainConfig &swapchain_config);

#ifdef VKL_HAS_VMA
/*!
 *  Initializes the framework.
 *  This overload additionally takes a VmaAllocator handle
 */
bool vklInitFramework(VkInstance vk_instance, VkSurfaceKHR vk_surface, VkPhysicalDevice vk_physical_device,
                      VkDevice vk_device, VkQueue vk_queue, const VklSwapchainConfig &swapchain_config,
                      VmaAllocator vma_allocator);
#endif 

/*!
 * Returns true if the framework has been properly initialized, false otherwise.
 * The framework can be initialized by one (and only one) call to vklInitFramework.
 */
bool vklFrameworkInitialized();

/*!
 * This function waits for the next swapchain image to become available, turns it into the
 * back buffer, so that the application can render into it.
 *
 * The technical details of this function include:
 *  - waiting for a VkFence,
 *	- calling vkAcquireNextImageKHR,
 *	- and signaling a VkSemaphore which indicates when the image has become available.
 *
 *	@return	The elapsed time that this function waited, measured with calls to glfwGetTime
 */
double vklWaitForNextSwapchainImage();

/*!
 * This function waits until rendering has finished and afterwards, presents the rendered image on the surface.
 *
 * The technical details of this function include:
 *  - signaling a VkSemaphore after all rendering has finished,
 *	- and a call to vkPresentKHR.
 */
void vklPresentCurrentSwapchainImage();

/*!
 *	This function internally creates a (single use) command buffer which will be recording until
 *	vklEndRecordingCommands() is called. Between the two, draw calls such can
 *	be recorded into the command buffer.
 *
 *	Valid usage: * [0..1] invocations of this function per frame
 *	             * Invocation count must match the invocation count of vklEndRecordingCommands()
 *				 * vklStartRecordingCommands() must be invoked before vklEndRecordingCommands()
 */
void vklStartRecordingCommands();

/*!
 *	This function ends recording into the command buffer (what has been started with a call to
 *	vklStartRecordingCommands() and been active until now) and submits the command buffer to the queue.
 *
 *	Valid usage: * [0..1] invocations of this function per frame
 *	             * Invocation count must match the invocation count of vklStartRecordingCommands()
 *				 * vklStartRecordingCommands() must be invoked after vklEndRecordingCommands()
 */
void vklEndRecordingCommands();

/*!
 * This function creates a new VkPipeline with some default settings and some settings from the given configuration struct.
 *
 * The settings taken from the configuration struct are the following:
 *  - ::vertexShaderPath ............. A path to a text file containing GLSL vertex shader code. If loadShadersFromMemory is true, then this is treated as the vertex shader code itself, not a path to it.
 *	- ::fragmentShaderPath ........... A path to a text file containing GLSL fragment shader code. If loadShadersFromMemory is true, then this is treated as the fragment shader code itself, not a path to it.
 *	- ::vertexInputBuffers ........... A list of descriptions for the structure of buffers, which contain data for vertex input attributes.
 *	- ::inputAttributeDescriptions ... A list of descriptions for vertex input attributes, and where the data can be found in ::vertexInputBuffers.
 *  - ::polygonDrawMode .............. How the graphics pipeline shall draw the input polygons.
 *	- ::descriptorLayout ............. A list of resource descriptors. Can be used for, e.g., describing where shaders can find uniform buffers.
 *
 * Settings which are configured to default values internally are the following:
 *  - The topology of the input data is expected to be a triangle list, i.e., VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST.
 *	- The viewport is set to (0, 0) -- (width, height), where width and height refer to the swap chain's width and height.
 *	- Min- and max depth values are set to 0.0, and 1.0, respectively.
 *	- Scissors are set to (0, 0) -- (width, height), where width and height refer to the swap chain's width and height.
 *	- The line width is set to 1.0
 *	- The culling mode is set to cull back-facing primitives, i.e., VK_CULL_MODE_BACK_BIT.
 *	- The front faces are set to be given in counter-clockwise winding order, i.e., VK_FRONT_FACE_COUNTER_CLOCKWISE.
 *	- The multisample state is set to 1 sample, i.e., VK_SAMPLE_COUNT_1_BIT.
 *	- Depth testing is enabled.
 *	- Depth writing is enabled.
 *	- Depth compare mode is set to VK_COMPARE_OP_LESS.
 *	- Color blending is disabled.
 *	- The color write mask is configured to write all color channels and the alpha channel.
 *
 *	@param	config		Configuration struct containing the non-default settings described above.
 *	@param loadShadersFromMemory If true, then the shader paths of the config struct are interpreted as shader code.
 *	@return On success, a valid VkPipeline handle is returned.
 */
VkPipeline vklCreateGraphicsPipeline(const VklGraphicsPipelineConfig &config, bool loadShadersFromMemory = false);

/*!
 *	Destroys a graphics pipeline that has been previously created with vklCreateGraphicsPipeline.
 *
 *	@param	pipeline	A valid handle to a graphics pipeline that has been created with vklCreateGraphicsPipeline.
 *						The pipeline will be unusable after this function has returned. 
 */
void vklDestroyGraphicsPipeline(VkPipeline pipeline);

/*!
 *  Allocates host-coherent memory that fits the given requirements.
 *
 *  @param bufferSize            The requested size of the memory in bytes.
 *  @param memoryRequirements    The requested requirements for the memory.
 *  @param memoryPropertyFlags   The memory properties that the allocated buffer must support. 
 *                               For, e.g., host-coherent memory, pass VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
 *                               For, e.g., device-local memory, pass VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
 */
VkDeviceMemory vklAllocateMemoryForGivenRequirements(VkDeviceSize bufferSize, VkMemoryRequirements memoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags);

/*!
 *	Creates a new buffer (VkBuffer) and also allocates new memory on the device (VkDeviceMemory) to back the
 *	buffer's size requirements. The memory will always be allocated from a region of so called "host coherent"
 *	memory, which is a region that is also accessible from the CPU, so that the CPU can write data into it.
 *	Internally, this is indicated with the flags VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
 *
 *	@param buffer_size	The requested size of the buffer in bytes.
 *	@param buffer_usage	Requested buffer usage flags.
 *
 *	@return A handle to a newly created buffer with backing memory.
 */
VkBuffer vklCreateHostCoherentBufferWithBackingMemory(VkDeviceSize buffer_size, VkBufferUsageFlags buffer_usage);

/*!
 *	Creates a new buffer (VkBuffer) and also allocates new memory on the device (VkDeviceMemory) to back the
 *	buffer's size requirements. The memory will always be allocated from a region of so called "device-local"
 *	memory, which is a region that is not accessible from the CPU, which is faster to access and transfer on the device.
 *	Internally, this is indicated with the flag VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT.
 *
 *	@param buffer_size	The requested size of the buffer in bytes.
 *	@param buffer_usage	Requested buffer usage flags.
 *
 *	@return A handle to a newly created buffer with backing memory.
 */
VkBuffer vklCreateDeviceLocalBufferWithBackingMemory(VkDeviceSize buffer_size, VkBufferUsageFlags buffer_usage);

/*!
 *	Frees the memory (VkDeviceMemory) and destroys the buffer (VkBuffer) which has previously been created
 *	using vklCreateHostCoherentBufferWithBackingMemory.
 *	@param	buffer		The buffer which shall be destroyed. The assigned VkDeviceMemory handle is tracked
 *						internally and will be freed before the buffer is destroyed.
 */
void vklDestroyHostCoherentBufferAndItsBackingMemory(VkBuffer buffer);

/*!
 *	Frees the memory (VkDeviceMemory) and destroys the buffer (VkBuffer) which has previously been created
 *	using vklCreateDeviceBufferWithBackingMemory.
 *	@param	buffer		The buffer which shall be destroyed. The assigned VkDeviceMemory handle is tracked
 *						internally and will be freed before the buffer is destroyed.
 */
void vklDestroyDeviceLocalBufferAndItsBackingMemory(VkBuffer buffer);

/*!
 *	Copies data into the buffer, by reading it from the address at data_pointer and of the given byte size.
 *
 *	@param	buffer				Host coherent buffer to copy data into.
 *	@param	data_pointer		Pointer to the beginning of CPU-side data.
 *	@param	data_size_in_bytes	How many bytes shall be copied from the memory address at data_pointer into the buffer?
 */
void vklCopyDataIntoHostCoherentBuffer(VkBuffer buffer, const void *data_pointer, size_t data_size_in_bytes);

/*!
 *	Copies data into the buffer to a given offset, by reading it from the address at data_pointer and of the given byte size.
 *
 *	@param	buffer					Host coherent buffer to copy data into.
 *	@param	buffer_offset_in_bytes	Offset from the beginning of buffer, where to start copying data to.
 *	@param	data_pointer			Pointer to the beginning of CPU-side data.
 *	@param	data_size_in_bytes		How many bytes shall be copied from the memory address at data_pointer into the buffer?
 */
void vklCopyDataIntoHostCoherentBuffer(VkBuffer buffer, size_t buffer_offset_in_bytes, const void *data_pointer,
                                       size_t data_size_in_bytes);

/*!
 * Create a new host coherent buffer on the GPU, upload the supplied data from the vector, and return the buffer handle.
 *
 * Be sure to free the allocated memory by calling `vklDestroyHostCoherentBufferAndItsBackingMemory(...)` on the returned handle once the
 * buffer is no longer required.
 *
 * @param data Pointer to the data to upload to the GPU.
 * @param size Size of the data in bytes.
 * @param usageFlags Usage flags to use when creating the buffer.
 * @return The handle of the newly generated buffer.
 */
VkBuffer vklCreateHostCoherentBufferAndUploadData(const void* data, size_t size, VkBufferUsageFlags usageFlags);

/*!
 *	Binds the given descriptor set for the given graphics pipeline (internally using vkCmdBindDescriptorSets).
 *	To be more precise: The VkPipelineLayout of the given VkPipeline is retrieved and the descriptor is bound for that.
 *
 *	@param	descriptor_set		This handle must represent a valid descriptor set.
 *								It will be bound to the VK_PIPELINE_BIND_POINT_GRAPHICS binding point.
 *	@param	pipeline			This handle must represent a valid graphics pipeline that has been created with 
 *								vklCreateGraphicsPipeline previously. Internally, its pipeline layout will be used.
 */
void vklBindDescriptorSetToPipeline(VkDescriptorSet descriptor_set, VkPipeline pipeline);

/*!
 *	Creates a 2D image (VkImage) of the given size, in the given format, and for the given usage(s) on the device.
 *	Also creates backing memory (VkDeviceMemory) for that image in device local memory (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).
 *
 *	@param	physical_device		The physical device where to create image and memory.
 *	@param	device				The device handle to be used for image and memory creation.
 *	@param	width				Image width.
 *	@param	height				Image height.
 *	@param	format				Image format (i.e., data format of each of the image's fragments)
 *	@param	usage_flags			Usage(s) which the newly created image can be used for.
 *
 *	@return A handle to a newly created image with backing memory.
 */
VkImage
vklCreateDeviceLocalImageWithBackingMemory(VkPhysicalDevice physical_device, VkDevice device, uint32_t width, uint32_t height,
                                           VkFormat format, VkImageUsageFlags usage_flags);

/*!
 *	Creates a 2D image (VkImage) of the given size, in the given format, and for the given usage(s) on the device.
 *	Also creates backing memory (VkDeviceMemory) for that image in device local memory (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).
 *
 *	@param	physical_device		The physical device where to create image and memory.
 *	@param	device				The device handle to be used for image and memory creation.
 *	@param	width				Image width.
 *	@param	height				Image height.
 *	@param	format				Image format (i.e., data format of each of the image's fragments)
 *	@param	usage_flags			Usage(s) which the newly created image can be used for.
 *	@param	array_layers		How many layers the image shall be created with
 *	@param	flags				Additional VkImageCreateFlagBits flags, describing additional parameters of the image.
 *
 *	@return A handle to a newly created image with backing memory.
 */
VkImage
vklCreateDeviceLocalImageWithBackingMemory(VkPhysicalDevice physical_device, VkDevice device, uint32_t width, uint32_t height,
                                           VkFormat format, VkImageUsageFlags usage_flags, uint32_t array_layers,
                                           VkImageCreateFlags flags);

/*!
 *	Frees the memory (VkDeviceMemory) and destroys the image (VkImage) which has previously been created
 *	using vklCreateDeviceLocalImageWithBackingMemory.
 *	@param	image		The image which shall be destroyed. The assigned VkDeviceMemory handle is tracked
 *						internally and will be freed before the image is destroyed.
 */
void vklDestroyDeviceLocalImageAndItsBackingMemory(VkImage image);

/*!
 *	Creates a 2D image (VkImage) of the given size, in the given format, and for the given usage(s) on the device.
 *	Also creates backing memory (VkDeviceMemory) for that image in device local memory (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).
 *
 *	@param	width				Image width.
 *	@param	height				Image height.
 *	@param	format				Image format (i.e., data format of each of the image's fragments)
 *	@param	usage_flags			Usage(s) which the newly created image can be used for.
 *
 *	@return A handle to a newly created image with backing memory.
 */
VkImage
vklCreateDeviceLocalImageWithBackingMemory(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage_flags);

/*!
 *	Creates a 2D image (VkImage) of the given size, in the given format, and for the given usage(s) on the device.
 *	Also creates backing memory (VkDeviceMemory) for that image in device local memory (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).
 *
 *	@param	width				Image width.
 *	@param	height				Image height.
 *	@param	format				Image format (i.e., data format of each of the image's fragments)
 *	@param	usage_flags			Usage(s) which the newly created image can be used for.
 *	@param	array_layers		How many layers the image shall be created with
 *	@param	flags				Additional VkImageCreateFlagBits flags, describing additional parameters of the image.
 *
 *	@return A handle to a newly created image with backing memory.
 */
VkImage vklCreateDeviceLocalImageWithBackingMemory(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage_flags,
                                                   uint32_t array_layers, VkImageCreateFlags flags);

/*!
 *	Gets the VkPipelineLayout for the given VkPipeline, given that the
 *	VkPipeline has been generated with vklCreateGraphicsPipeline previously.
 *	The pipeline layout of pipelines created with vklCreateGraphicsPipeline
 *	are stored internally and can be retrieved using this function.
 *	
 *	@param	pipeline	A valid VkPipeline handle which has been created with vklCreateGraphicsPipeline
 *	@return	The VkPipelineLayout handle that was used to create the given pipeline.
 */
VkPipelineLayout vklGetLayoutForPipeline(VkPipeline pipeline);

/*!
 *	Returns the currently swap chain image index which has been set during the
 *	last call to vklWaitForNextSwapchainImage() when acquiring the next swap chain
 *	image.
 */
uint32_t vklGetCurrentSwapChainImageIndex();

/*!
 *	Returns the number of framebuffers that are used by the framework. This will most
 *	likely be at least two (a front buffer and a back buffer) and matches the
 *	number of swap chain images that have been created.
 */
uint32_t vklGetNumFramebuffers();

/*!
 *  Returns the number of clear values currently in use by the
 *  framework. This should correspond to the number of swapchain
 *  images.
 */
uint32_t vklGetNumClearValues();

/*!
 *	Returns the framebuffers at the given index. The index is bounded by the number 
 *	of swap chain images (see vklGetNumFramebuffers()).
 */
VkFramebuffer vklGetFramebuffer(uint32_t i);

/*!
 *	Returns the currently active framebuffer that is used as back buffer, i.e., which
 *	is to be rendered into during the current frame.
 */
VkFramebuffer vklGetCurrentFramebuffer();

/*!
 *	Returns the render pass which was used to create the framebuffers.
 */
VkRenderPass vklGetRenderpass();

/*!
 *	Returns the currently active command buffer (if there is one).
 *	The command buffer that is returned is the one that was created during the last
 *	call to vklStartRecordingCommands(). It will remain active until the next call
 *	to vklEndRecordingCommands().
 * 
 *	The command buffer returned has already begun to record commands. It will continue
 *	recording commands until the next call to vklEndRecordingCommands(). Also during 
 *	the next call to vklEndRecordingCommands(), it will be submitted to the queue.
 */
VkCommandBuffer vklGetCurrentCommandBuffer();

/*!
 *  Returns the basic vulkan pipeline automatically set up by the
 *  framework.
 */
VkPipeline vklGetBasicPipeline();

/*!
 *  Returns the device chosen by the framework.
 */
VkDevice vklGetDevice();

/*!
 * Destroys the framework
 */
void vklDestroyFramework();

/*!
 *	A struct containing image information
 */
struct VklImageInfo {
    /*! The format of the image: */
    VkFormat imageFormat;

    /*! Width and height of the image: */
    VkExtent2D extent;
};

/*!
 *	Determines image information about the DDS image file at the given path.
 *	This is equivalent to calling vklGetDdsImageLevelInfo with mipmap level = 0.
 *
 *	@param	file	Path to a DDS image file
 *	@return	A struct containing information about the DDS image file.
 */
VklImageInfo vklGetDdsImageInfo(const char *file);

/*!
 *	Determines image information about a specific mipmap level of
 *	the DDS image file at the given path.
 *
 *	@param	file	Path to a DDS image file
 *	@param	level	The mipmap level which info to determine and return.
 *	@return	A struct containing information about the DDS image file.
 */
VklImageInfo vklGetDdsImageLevelInfo(const char* file, uint32_t level);

/*!
 *	Loads a DDS image from a file directly into a host-coherent buffer.
 *	To cleanup, pass the returned buffer handle to vklDestroyHostCoherentBufferAndItsBackingMemory!
 *	This is equivalent to calling vklLoadDdsImageFaceLevelIntoHostCoherentBuffer with face-id = 0, and mipmap level = 0.
 *
 *	@param	file	Path to a DDS image file
 *	@return	A newly created buffer in host-coherent memory which contains the data of the given DDS image file.
 */
VkBuffer vklLoadDdsImageIntoHostCoherentBuffer(const char* file);

/*!
 *	Loads one particular mipmap level of a DDS image from a file directly into a host-coherent buffer.
 *	This is equivalent to calling vklLoadDdsImageFaceLevelIntoHostCoherentBuffer with face-id = 0 and the respective level.
 *	To cleanup, pass the returned buffer handle to vklDestroyHostCoherentBufferAndItsBackingMemory!
 *
 *	@param	file	Path to a DDS image file
 *	@param	level	The mipmap level which to load into the buffer (i.e., this one and only this one)
 *	@return	A newly created buffer in host-coherent memory which contains the data of the given DDS image file.
 */
VkBuffer vklLoadDdsImageLevelIntoHostCoherentBuffer(const char* file, uint32_t level);

/*!
 *	Loads one particular mipmap level of a particular face of a DDS image from a file directly into a host-coherent buffer.
 *	To cleanup, pass the returned buffer handle to vklDestroyHostCoherentBufferAndItsBackingMemory!
 *
 *	@param	file	Path to a DDS image file
 *	@param	face	The face-id which to load into the buffer (i.e., this one and only this one)
 *	@param	level	The mipmap level which to load into the buffer (i.e., this one and only this one)
 *	@return	A newly created buffer in host-coherent memory which contains the data of the given DDS image file.
 */
VkBuffer vklLoadDdsImageFaceLevelIntoHostCoherentBuffer(const char* file, uint32_t face, uint32_t level);

/*!
 *	Creates a perspective projection matrix which transforms a part of the scene into a unit cube based on the given parameters.
 *	The scene is assumed to be given in a right-handed coordinate system with its y axis pointing upwards.
 *	The part of the scene that will end up within the unit cube is located towards the negative z axis.
 *	@param	field_of_view			The perspective projection's full field of view in radians.
 *	@param	aspect_ratio			The ratio of the screen's width to its height.
 *	@param	near_plane_distance		The distance from the camera's origin to the near plane.
 *	@param	far_plane_distance		The distance from the camera's origin to the far plane.
 *	@return	A perspective projection matrix based on the given parameters.
 */
glm::mat4 vklCreatePerspectiveProjectionMatrix(float field_of_view, float aspect_ratio, float near_plane_distance, float far_plane_distance);

/*!
 *  Loads a .obj model from the specified path and fills a VklGeometryData struct with the vertices, indices, normals and uv coordinates, if any exist.
 *  @param  path_to_obj    Path to a 3D model in .obj format, the geometry of which shall be loaded into host memory.
 *	                       Note: the .obj format is the only format that is supported. Trying to load a different 3D model format will fail.  
 *	@return A struct instance containing all the geometry data of the loaded 3D model 
 */
VklGeometryData vklLoadModelGeometry(const std::string& path_to_obj);
