#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <vulkan/vulkan.h>
enum vk_format_layout {
   /**
    * Formats with vk_format_block::width == vk_format_block::height == 1
    * that can be described as an ordinary data structure.
    */
   VK_FORMAT_LAYOUT_PLAIN = 0,

   /**
    * Formats with sub-sampled channels.
    *
    * This is for formats like YVYU where there is less than one sample per
    * pixel.
    */
   VK_FORMAT_LAYOUT_SUBSAMPLED = 3,

   /**
    * S3 Texture Compression formats.
    */
   VK_FORMAT_LAYOUT_S3TC = 4,

   /**
    * Red-Green Texture Compression formats.
    */
   VK_FORMAT_LAYOUT_RGTC = 5,

   /**
    * Ericsson Texture Compression
    */
   VK_FORMAT_LAYOUT_ETC = 6,

   /**
    * BC6/7 Texture Compression
    */
   VK_FORMAT_LAYOUT_BPTC = 7,

   /**
    * ASTC
    */
   VK_FORMAT_LAYOUT_ASTC = 8,

   /**
    * Everything else that doesn't fit in any of the above layouts.
    */
   VK_FORMAT_LAYOUT_OTHER = 9
};
   
struct vk_format_block
{
   /** Block width in pixels */
   unsigned width;
   
   /** Block height in pixels */
   unsigned height;

   /** Block size in bits */
   unsigned bits;
};
   
enum vk_format_type {
   VK_FORMAT_TYPE_VOID = 0,
   VK_FORMAT_TYPE_UNSIGNED = 1,
   VK_FORMAT_TYPE_SIGNED = 2,
   VK_FORMAT_TYPE_FIXED = 3,
   VK_FORMAT_TYPE_FLOAT = 4
};


enum vk_format_colorspace {
   VK_FORMAT_COLORSPACE_RGB = 0,
   VK_FORMAT_COLORSPACE_SRGB = 1,
   VK_FORMAT_COLORSPACE_YUV = 2,
   VK_FORMAT_COLORSPACE_ZS = 3
};

struct vk_format_channel_description {
   unsigned type:5;
   unsigned normalized:1;
   unsigned pure_integer:1;
   unsigned size:9;
   unsigned shift:16;
};

struct vk_format_description
{
   VkFormat format;
   const char *name;
   const char *short_name;

   struct vk_format_block block;
   enum vk_format_layout layout;

   unsigned nr_channels:3;
   unsigned is_array:1;
   unsigned is_bitmask:1;
   unsigned is_mixed:1;

   struct vk_format_channel_description channel[4];

   unsigned char swizzle[4];

   enum vk_format_colorspace colorspace;
};

extern const struct vk_format_description vk_format_description_table[];

const struct vk_format_description *vk_format_description(VkFormat format);

/**
 * Return total bits needed for the pixel format per block.
 */
static inline uint
vk_format_get_blocksizebits(VkFormat format)
{
   const struct vk_format_description *desc = vk_format_description(format);

   assert(desc);
   if (!desc) {
      return 0;
   }

   return desc->block.bits;
}

/**
 * Return bytes per block (not pixel) for the given format.
 */
static inline uint
vk_format_get_blocksize(VkFormat format)
{
   uint bits = vk_format_get_blocksizebits(format);
   uint bytes = bits / 8;

   assert(bits % 8 == 0);
   assert(bytes > 0);
   if (bytes == 0) {
      bytes = 1;
   }

   return bytes;
}

static inline uint
vk_format_get_blockwidth(VkFormat format)
{
   const struct vk_format_description *desc = vk_format_description(format);

   assert(desc);
   if (!desc) {
      return 1;
   }

   return desc->block.width;
}

static inline uint
vk_format_get_blockheight(VkFormat format)
{
   const struct vk_format_description *desc = vk_format_description(format);

   assert(desc);
   if (!desc) {
      return 1;
   }

   return desc->block.height;
}

/**
 * Return the index of the first non-void channel
 * -1 if no non-void channels
 */
static inline int
vk_format_get_first_non_void_channel(VkFormat format)
{
   const struct vk_format_description *desc = vk_format_description(format);
   int i;

   for (i = 0; i < 4; i++)
      if (desc->channel[i].type != VK_FORMAT_TYPE_VOID)
         break;

   if (i == 4)
       return -1;

   return i;
}

enum vk_swizzle {
   VK_SWIZZLE_X,
   VK_SWIZZLE_Y,
   VK_SWIZZLE_Z,
   VK_SWIZZLE_W,
   VK_SWIZZLE_0,
   VK_SWIZZLE_1,
   VK_SWIZZLE_NONE,
   VK_SWIZZLE_MAX, /**< Number of enums counter (must be last) */
};

#ifdef __cplusplus
} // extern "C" {
#endif
