#include "radv_private.h"

#include "vk_format.h"
#include "sid.h"

uint32_t radv_translate_buffer_dataformat(const struct vk_format_description *desc,
					  int first_non_void)
{
    unsigned type;
    int i;

    assert(first_non_void >= 0);
    type = desc->channel[first_non_void].type;

    if (type == VK_FORMAT_TYPE_FIXED)
	return V_008F0C_BUF_DATA_FORMAT_INVALID;
    if (desc->nr_channels == 4 &&
	desc->channel[0].size == 10 &&
	desc->channel[1].size == 10 &&
	desc->channel[2].size == 10 &&
	desc->channel[3].size == 2)
	return V_008F0C_BUF_DATA_FORMAT_2_10_10_10;

    /* See whether the components are of the same size. */
    for (i = 0; i < desc->nr_channels; i++) {
	if (desc->channel[first_non_void].size != desc->channel[i].size)
	    return V_008F0C_BUF_DATA_FORMAT_INVALID;
    }

    switch (desc->channel[first_non_void].size) {
    case 8:
	switch (desc->nr_channels) {
	case 1:
	    return V_008F0C_BUF_DATA_FORMAT_8;
	case 2:
	    return V_008F0C_BUF_DATA_FORMAT_8_8;
	case 3:
	case 4:
	    return V_008F0C_BUF_DATA_FORMAT_8_8_8_8;
	}
	break;
    case 16:
	switch (desc->nr_channels) {
	case 1:
	    return V_008F0C_BUF_DATA_FORMAT_16;
	case 2:
	    return V_008F0C_BUF_DATA_FORMAT_16_16;
	case 3:
	case 4:
	    return V_008F0C_BUF_DATA_FORMAT_16_16_16_16;
	}
	break;
    case 32:
	/* From the Southern Islands ISA documentation about MTBUF:
	 * 'Memory reads of data in memory that is 32 or 64 bits do not
	 * undergo any format conversion.'
	 */
	if (type != VK_FORMAT_TYPE_FLOAT &&
	    !desc->channel[first_non_void].pure_integer)
	    return V_008F0C_BUF_DATA_FORMAT_INVALID;

	switch (desc->nr_channels) {
	case 1:
	    return V_008F0C_BUF_DATA_FORMAT_32;
	case 2:
	    return V_008F0C_BUF_DATA_FORMAT_32_32;
	case 3:
	    return V_008F0C_BUF_DATA_FORMAT_32_32_32;
	case 4:
	    return V_008F0C_BUF_DATA_FORMAT_32_32_32_32;
	}
	break;
    }

    return V_008F0C_BUF_DATA_FORMAT_INVALID;
}

uint32_t radv_translate_buffer_numformat(const struct vk_format_description *desc,
					 int first_non_void)
{
    //	if (desc->format == PIPE_FORMAT_R11G11B10_FLOAT)
    //		return V_008F0C_BUF_NUM_FORMAT_FLOAT;

	assert(first_non_void >= 0);

	switch (desc->channel[first_non_void].type) {
	case VK_FORMAT_TYPE_SIGNED:
		if (desc->channel[first_non_void].normalized)
			return V_008F0C_BUF_NUM_FORMAT_SNORM;
		else if (desc->channel[first_non_void].pure_integer)
			return V_008F0C_BUF_NUM_FORMAT_SINT;
		else
			return V_008F0C_BUF_NUM_FORMAT_SSCALED;
		break;
	case VK_FORMAT_TYPE_UNSIGNED:
		if (desc->channel[first_non_void].normalized)
			return V_008F0C_BUF_NUM_FORMAT_UNORM;
		else if (desc->channel[first_non_void].pure_integer)
			return V_008F0C_BUF_NUM_FORMAT_UINT;
		else
			return V_008F0C_BUF_NUM_FORMAT_USCALED;
		break;
	case VK_FORMAT_TYPE_FLOAT:
	default:
		return V_008F0C_BUF_NUM_FORMAT_FLOAT;
	}
}

static uint32_t radv_translate_texformat(struct radv_physical_device *physical_device,
					 VkFormat format,
					 const struct vk_format_description *desc,
					 int first_non_void)
{
   bool uniform = true;
   int i;

   if (!desc)
      return ~0;
   /* Colorspace (return non-RGB formats directly). */
   switch (desc->colorspace) {
      /* Depth stencil formats */
   case VK_FORMAT_COLORSPACE_ZS:
      switch (format) {
      case VK_FORMAT_D16_UNORM:
	 return V_008F14_IMG_DATA_FORMAT_16;
      case VK_FORMAT_D24_UNORM_S8_UINT:
	 return V_008F14_IMG_DATA_FORMAT_8_24;
      case VK_FORMAT_S8_UINT:
	 return V_008F14_IMG_DATA_FORMAT_8;
      case VK_FORMAT_D32_SFLOAT:
	 return V_008F14_IMG_DATA_FORMAT_32;
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
	 return V_008F14_IMG_DATA_FORMAT_X24_8_32;
      default:
	 goto out_unknown;
      }

   case VK_FORMAT_COLORSPACE_YUV:
      goto out_unknown; /* TODO */

   case VK_FORMAT_COLORSPACE_SRGB:
      if (desc->nr_channels != 4 && desc->nr_channels != 1)
	 goto out_unknown;
      break;

   default:
      break;
   }

   if (format == VK_FORMAT_E5B9G9R9_UFLOAT_PACK32) {
      return V_008F14_IMG_DATA_FORMAT_5_9_9_9;
   } else if (format == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
      return V_008F14_IMG_DATA_FORMAT_10_11_11;
   }

   /* R8G8Bx_SNORM - TODO CxV8U8 */

   /* hw cannot support mixed formats (except depth/stencil, since only
    * depth is read).*/
   if (desc->is_mixed && desc->colorspace != VK_FORMAT_COLORSPACE_ZS)
      goto out_unknown;

   /* See whether the components are of the same size. */
   for (i = 1; i < desc->nr_channels; i++) {
      uniform = uniform && desc->channel[0].size == desc->channel[i].size;
   }

   /* Non-uniform formats. */
   if (!uniform) {
      switch(desc->nr_channels) {
      case 3:
	 if (desc->channel[0].size == 5 &&
	     desc->channel[1].size == 6 &&
	     desc->channel[2].size == 5) {
	    return V_008F14_IMG_DATA_FORMAT_5_6_5;
	 }
	 goto out_unknown;
      case 4:
	 if (desc->channel[0].size == 5 &&
	     desc->channel[1].size == 5 &&
	     desc->channel[2].size == 5 &&
	     desc->channel[3].size == 1) {
	    return V_008F14_IMG_DATA_FORMAT_1_5_5_5;
	 }
	 if (desc->channel[0].size == 10 &&
	     desc->channel[1].size == 10 &&
	     desc->channel[2].size == 10 &&
	     desc->channel[3].size == 2) {
	    return V_008F14_IMG_DATA_FORMAT_2_10_10_10;
	 }
	 goto out_unknown;
      }
      goto out_unknown;
   }

   if (first_non_void < 0 || first_non_void > 3)
      goto out_unknown;

   /* uniform formats */
   switch (desc->channel[first_non_void].size) {
   case 4:
      switch (desc->nr_channels) {
#if 0 /* Not supported for render targets */
      case 2:
	 return V_008F14_IMG_DATA_FORMAT_4_4;
#endif
      case 4:
	 return V_008F14_IMG_DATA_FORMAT_4_4_4_4;
      }
      break;
   case 8:
      switch (desc->nr_channels) {
      case 1:
	 return V_008F14_IMG_DATA_FORMAT_8;
      case 2:
	 return V_008F14_IMG_DATA_FORMAT_8_8;
      case 4:
	 return V_008F14_IMG_DATA_FORMAT_8_8_8_8;
      }
      break;
   case 16:
      switch (desc->nr_channels) {
      case 1:
	 return V_008F14_IMG_DATA_FORMAT_16;
      case 2:
	 return V_008F14_IMG_DATA_FORMAT_16_16;
      case 4:
	 return V_008F14_IMG_DATA_FORMAT_16_16_16_16;
      }
      break;
   case 32:
      switch (desc->nr_channels) {
      case 1:
	 return V_008F14_IMG_DATA_FORMAT_32;
      case 2:
	 return V_008F14_IMG_DATA_FORMAT_32_32;
#if 0 /* Not supported for render targets */
      case 3:
	 return V_008F14_IMG_DATA_FORMAT_32_32_32;
#endif
      case 4:
	 return V_008F14_IMG_DATA_FORMAT_32_32_32_32;
      }
   }

 out_unknown:
   /* R600_ERR("Unable to handle texformat %d %s\n", format, vk_format_name(format)); */
   return ~0;
}

static bool radv_is_sampler_format_supported(struct radv_physical_device *physical_device,
					     VkFormat format)
{
   struct vk_format_description *desc = vk_format_description(format);
   if (!desc || format == VK_FORMAT_UNDEFINED)
      return false;
   return radv_translate_texformat(physical_device, format, vk_format_description(format),
				   vk_format_get_first_non_void_channel(format)) != ~0U;
}

static void
radv_physical_device_get_format_properties(struct radv_physical_device *physical_device,
                                          VkFormat format,
                                          VkFormatProperties *out_properties)
{
   VkFormatFeatureFlags linear = 0, tiled = 0, buffer = 0;
   
   if (radv_is_sampler_format_supported(physical_device, format)) {
      linear |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
	 VK_FORMAT_FEATURE_BLIT_SRC_BIT;
      tiled |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
	 VK_FORMAT_FEATURE_BLIT_SRC_BIT;
   }
   out_properties->linearTilingFeatures = linear;
   out_properties->optimalTilingFeatures = tiled;
   out_properties->bufferFeatures = buffer;
}

void radv_GetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
   RADV_FROM_HANDLE(radv_physical_device, physical_device, physicalDevice);

   radv_physical_device_get_format_properties(physical_device,
					      format,
					      pFormatProperties);
}

VkResult radv_GetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          createFlags,
    VkImageFormatProperties*                    pImageFormatProperties)
{
   RADV_FROM_HANDLE(radv_physical_device, physical_device, physicalDevice);
   VkFormatProperties format_props;
   VkFormatFeatureFlags format_feature_flags;
   VkExtent3D maxExtent;
   uint32_t maxMipLevels;
   uint32_t maxArraySize;
   VkSampleCountFlags sampleCounts = VK_SAMPLE_COUNT_1_BIT;

   radv_physical_device_get_format_properties(physical_device, format,
                                             &format_props);

   return VK_SUCCESS;
}
      
void radv_GetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    uint32_t                                    samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pNumProperties,
    VkSparseImageFormatProperties*              pProperties)
{
   /* Sparse images are not yet supported. */
   *pNumProperties = 0;
}
