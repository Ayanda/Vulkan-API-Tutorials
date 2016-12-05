/* -----------------------------------------------------
This source code is public domain ( CC0 )
The code is provided as-is without limitations, requirements and responsibilities.
Creators and contributors to this source code are provided as a token of appreciation
and no one associated with this source code can be held responsible for any possible
damages or losses of any kind.

Original file creator:  Niko Kauppi (Code maintenance)
Contributors:
----------------------------------------------------- */

#include "Window.h"
#include "Renderer.h"
#include "Shared.h"

#include <array>		// 10: include array
#include <assert.h>

Window::Window( Renderer * renderer, uint32_t size_x, uint32_t size_y, std::string name )
{
	_renderer			= renderer;
	_surface_size_x		= size_x;
	_surface_size_y		= size_y;
	_window_name		= name;

	_InitOSWindow();
	_InitSurface();
	_InitSwapchain();
	_InitSwapchainImages();
	_InitDepthStencilImage();
	_InitRenderPass();			// 3: Add _InitRenderPass to constructor
}

Window::~Window()
{
	_DeInitRenderPass();		// 4: Add _DeInitRenderPass to destructor
	_DeInitDepthStencilImage();
	_DeInitSwapchainImages();
	_DeInitSwapchain();
	_DeInitSurface();
	_DeInitOSWindow();
}


void Window::Close()
{
	_window_should_run		= false;
}

bool Window::Update()
{
	_UpdateOSWindow();
	return _window_should_run;
}

void Window::_InitSurface()
{
	_InitOSSurface();

	auto gpu = _renderer->GetVulkanPhysicalDevice();

	VkBool32 WSI_supported = false;
	vkGetPhysicalDeviceSurfaceSupportKHR( gpu, _renderer->GetVulkanGraphicsQueueFamilyIndex(), _surface, &WSI_supported );
	if( !WSI_supported ) {
		assert( 0 && "WSI not supported" );
		std::exit( -1 );
	}

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gpu, _surface, &_surface_capabilities );
	if( _surface_capabilities.currentExtent.width < UINT32_MAX ) {
		_surface_size_x			= _surface_capabilities.currentExtent.width;
		_surface_size_y			= _surface_capabilities.currentExtent.height;
	}

	{
		uint32_t format_count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR( gpu, _surface, &format_count, nullptr );
		if( format_count == 0 ) {
			assert( 0 && "Surface formats missing." );
			std::exit( -1 );
		}
		std::vector<VkSurfaceFormatKHR> formats( format_count );
		vkGetPhysicalDeviceSurfaceFormatsKHR( gpu, _surface, &format_count, formats.data() );
		if( formats[ 0 ].format == VK_FORMAT_UNDEFINED ) {
			_surface_format.format		= VK_FORMAT_B8G8R8A8_UNORM;
			_surface_format.colorSpace	= VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		} else {
			_surface_format				= formats[ 0 ];
		}
	}
}

void Window::_DeInitSurface()
{
	vkDestroySurfaceKHR( _renderer->GetVulkanInstance(), _surface, nullptr );
}

void Window::_InitSwapchain()
{
	// This code is old and the fixed one is below
	// if( _swapchain_image_count > _surface_capabilities.maxImageCount ) _swapchain_image_count = _surface_capabilities.maxImageCount;
	// if( _swapchain_image_count < _surface_capabilities.minImageCount + 1 ) _swapchain_image_count = _surface_capabilities.minImageCount + 1;

	// The code above will work just fine in our tutorials and likely on every possible implementation of vulkan as well
	// so this change isn't that important. Just to be absolutely sure we don't go over or below the given limits we should check this a
	// little bit different though. maxImageCount can actually be zero in which case the amount of swapchain images do not have an
	// upper limit other than available memory. It's also possible that the swapchain image amount is locked to a certain
	// value on certain systems. The code below takes into consideration both of these possibilities.
	if( _swapchain_image_count < _surface_capabilities.minImageCount + 1 ) _swapchain_image_count = _surface_capabilities.minImageCount + 1;
	if( _surface_capabilities.maxImageCount > 0 ) {
		if( _swapchain_image_count > _surface_capabilities.maxImageCount ) _swapchain_image_count = _surface_capabilities.maxImageCount;
	}

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	{
		uint32_t present_mode_count = 0;
		ErrorCheck( vkGetPhysicalDeviceSurfacePresentModesKHR( _renderer->GetVulkanPhysicalDevice(), _surface, &present_mode_count, nullptr ) );
		std::vector<VkPresentModeKHR> present_mode_list( present_mode_count );
		ErrorCheck( vkGetPhysicalDeviceSurfacePresentModesKHR( _renderer->GetVulkanPhysicalDevice(), _surface, &present_mode_count, present_mode_list.data() ) );
		for( auto m : present_mode_list ) {
			if( m == VK_PRESENT_MODE_MAILBOX_KHR ) present_mode = m;
		}
	}

	VkSwapchainCreateInfoKHR swapchain_create_info {};
	swapchain_create_info.sType						= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface					= _surface;
	swapchain_create_info.minImageCount				= _swapchain_image_count;
	swapchain_create_info.imageFormat				= _surface_format.format;
	swapchain_create_info.imageColorSpace			= _surface_format.colorSpace;
	swapchain_create_info.imageExtent.width			= _surface_size_x;
	swapchain_create_info.imageExtent.height		= _surface_size_y;
	swapchain_create_info.imageArrayLayers			= 1;
	swapchain_create_info.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_create_info.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	swapchain_create_info.queueFamilyIndexCount		= 0;
	swapchain_create_info.pQueueFamilyIndices		= nullptr;
	swapchain_create_info.preTransform				= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_create_info.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode				= present_mode;
	swapchain_create_info.clipped					= VK_TRUE;
	swapchain_create_info.oldSwapchain				= VK_NULL_HANDLE;

	ErrorCheck( vkCreateSwapchainKHR( _renderer->GetVulkanDevice(), &swapchain_create_info, nullptr, &_swapchain ) );

	ErrorCheck( vkGetSwapchainImagesKHR( _renderer->GetVulkanDevice(), _swapchain, &_swapchain_image_count, nullptr ) );
}

void Window::_DeInitSwapchain()
{
	vkDestroySwapchainKHR( _renderer->GetVulkanDevice(), _swapchain, nullptr );
}

void Window::_InitSwapchainImages()
{
	_swapchain_images.resize( _swapchain_image_count );
	_swapchain_image_views.resize( _swapchain_image_count );

	ErrorCheck( vkGetSwapchainImagesKHR( _renderer->GetVulkanDevice(), _swapchain, &_swapchain_image_count, _swapchain_images.data() ) );

	for( uint32_t i=0; i < _swapchain_image_count; ++i ) {
		VkImageViewCreateInfo image_view_create_info {};
		image_view_create_info.sType				= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.image				= _swapchain_images[ i ];
		image_view_create_info.viewType				= VK_IMAGE_VIEW_TYPE_2D;
		image_view_create_info.format				= _surface_format.format;
		image_view_create_info.components.r			= VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.g			= VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.b			= VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.a			= VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.subresourceRange.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
		image_view_create_info.subresourceRange.baseMipLevel		= 0;
		image_view_create_info.subresourceRange.levelCount			= 1;
		image_view_create_info.subresourceRange.baseArrayLayer		= 0;
		image_view_create_info.subresourceRange.layerCount			= 1;

		ErrorCheck( vkCreateImageView( _renderer->GetVulkanDevice(), &image_view_create_info, nullptr, &_swapchain_image_views[ i ] ) );
	}
}

void Window::_DeInitSwapchainImages()
{
	for( auto view : _swapchain_image_views ) {
		vkDestroyImageView( _renderer->GetVulkanDevice(), view, nullptr );
	}
}

void Window::_InitDepthStencilImage()
{
	{
		std::vector<VkFormat> try_formats {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM
		};
		for( auto f : try_formats ) {
			VkFormatProperties format_properties {};
			vkGetPhysicalDeviceFormatProperties( _renderer->GetVulkanPhysicalDevice(), f, &format_properties );
			if( format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) {
				_depth_stencil_format = f;
				break;
			}
		}
		if( _depth_stencil_format == VK_FORMAT_UNDEFINED ) {
			assert( 0 && "Depth stencil format not selected." );
			std::exit( -1 );
		}
		if( ( _depth_stencil_format == VK_FORMAT_D32_SFLOAT_S8_UINT ) ||
			( _depth_stencil_format == VK_FORMAT_D24_UNORM_S8_UINT ) ||
			( _depth_stencil_format == VK_FORMAT_D16_UNORM_S8_UINT ) ||
			( _depth_stencil_format == VK_FORMAT_S8_UINT ) ) {
			_stencil_available				= true;
		}
	}

	VkImageCreateInfo image_create_info {};
	image_create_info.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.flags					= 0;
	image_create_info.imageType				= VK_IMAGE_TYPE_2D;
	image_create_info.format				= _depth_stencil_format;
	image_create_info.extent.width			= _surface_size_x;
	image_create_info.extent.height			= _surface_size_y;
	image_create_info.extent.depth			= 1;
	image_create_info.mipLevels				= 1;
	image_create_info.arrayLayers			= 1;
	image_create_info.samples				= VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling				= VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage					= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	image_create_info.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.queueFamilyIndexCount	= VK_QUEUE_FAMILY_IGNORED;
	image_create_info.pQueueFamilyIndices	= nullptr;
	image_create_info.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

	vkCreateImage( _renderer->GetVulkanDevice(), &image_create_info, nullptr, &_depth_stencil_image );

	VkMemoryRequirements image_memory_requirements {};
	vkGetImageMemoryRequirements( _renderer->GetVulkanDevice(), _depth_stencil_image, &image_memory_requirements );

	uint32_t memory_index					= FindMemoryTypeIndex( &_renderer->GetVulkanPhysicalDeviceMemoryProperties(), &image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VkMemoryAllocateInfo memory_allocate_info {};
	memory_allocate_info.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize		= image_memory_requirements.size;
	memory_allocate_info.memoryTypeIndex	= memory_index;

	vkAllocateMemory( _renderer->GetVulkanDevice(), &memory_allocate_info, nullptr, &_depth_stencil_image_memory );
	vkBindImageMemory( _renderer->GetVulkanDevice(), _depth_stencil_image, _depth_stencil_image_memory, 0 );

	VkImageViewCreateInfo image_view_create_info {};
	image_view_create_info.sType				= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.image				= _depth_stencil_image;
	image_view_create_info.viewType				= VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format				= _depth_stencil_format;
	image_view_create_info.components.r			= VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.g			= VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.b			= VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.a			= VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_DEPTH_BIT | ( _stencil_available ? VK_IMAGE_ASPECT_STENCIL_BIT : 0 );
	image_view_create_info.subresourceRange.baseMipLevel	= 0;
	image_view_create_info.subresourceRange.levelCount		= 1;
	image_view_create_info.subresourceRange.baseArrayLayer	= 0;
	image_view_create_info.subresourceRange.layerCount		= 1;

	vkCreateImageView( _renderer->GetVulkanDevice(), &image_view_create_info, nullptr, &_depth_stencil_image_view );
}

void Window::_DeInitDepthStencilImage()
{
	vkDestroyImageView( _renderer->GetVulkanDevice(), _depth_stencil_image_view, nullptr );
	vkFreeMemory( _renderer->GetVulkanDevice(), _depth_stencil_image_memory, nullptr );
	vkDestroyImage( _renderer->GetVulkanDevice(), _depth_stencil_image, nullptr );
}

void Window::_InitRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachment_descriptions {};			// 9: Create an array of attachment_descriptions
	attachment_descriptions[ 0 ].flags			= 0;
	attachment_descriptions[ 0 ].format			= _depth_stencil_format;
	attachment_descriptions[ 0 ].samples		= VK_SAMPLE_COUNT_1_BIT;
	attachment_descriptions[ 0 ].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_descriptions[ 0 ].storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	attachment_descriptions[ 0 ].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment_descriptions[ 0 ].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_STORE;
	attachment_descriptions[ 0 ].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachment_descriptions[ 0 ].finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	attachment_descriptions[ 1 ].flags			= 0;
	attachment_descriptions[ 1 ].format			= _surface_format.format;
	attachment_descriptions[ 1 ].samples		= VK_SAMPLE_COUNT_1_BIT;
	attachment_descriptions[ 1 ].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_descriptions[ 1 ].storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	attachment_descriptions[ 1 ].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment_descriptions[ 1 ].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_descriptions[ 1 ].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachment_descriptions[ 1 ].finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


	VkAttachmentReference subpass_0_depth_stencil_attref {};		// 12: Create subpass_0 depth stencil and color attachment references
	subpass_0_depth_stencil_attref.attachment	= 0;
	subpass_0_depth_stencil_attref.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 1> subpass_0_color_attrefs {};
	subpass_0_color_attrefs[ 0 ].attachment		= 1;
	subpass_0_color_attrefs[ 0 ].layout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDescription, 1> subpass_description {};			// 11: Create subpass_description
	subpass_description[ 0 ].flags						= 0;			// reserved for future use
	subpass_description[ 0 ].pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description[ 0 ].inputAttachmentCount		= 0;
	subpass_description[ 0 ].pInputAttachments			= nullptr;
	subpass_description[ 0 ].colorAttachmentCount		= subpass_0_color_attrefs.size();			// layout(location=0) out vec4 FinalColor;
	subpass_description[ 0 ].pColorAttachments			= subpass_0_color_attrefs.data();
	subpass_description[ 0 ].pResolveAttachments		= nullptr;
	subpass_description[ 0 ].pDepthStencilAttachment	= &subpass_0_depth_stencil_attref;
	subpass_description[ 0 ].preserveAttachmentCount	= 0;
	subpass_description[ 0 ].pPreserveAttachments		= nullptr;


	VkRenderPassCreateInfo render_pass_create_info {};		// 7: Add this struct
	render_pass_create_info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;				// 8: Fill out the fields
	render_pass_create_info.pNext				= nullptr;
	render_pass_create_info.flags				= 0;			// reserved for future use
	render_pass_create_info.attachmentCount		= attachment_descriptions.size();
	render_pass_create_info.pAttachments		= attachment_descriptions.data();
	render_pass_create_info.subpassCount		= subpass_description.size();
	render_pass_create_info.pSubpasses			= subpass_description.data();
	render_pass_create_info.dependencyCount		= 0;
	render_pass_create_info.pDependencies		= nullptr;

	ErrorCheck( vkCreateRenderPass( _renderer->GetVulkanDevice(), &render_pass_create_info, nullptr, &_render_pass ) );		// 5: Add vk function call
}

void Window::_DeInitRenderPass()
{
	vkDestroyRenderPass( _renderer->GetVulkanDevice(), _render_pass, nullptr );		// 13: Add destroyer function
}
