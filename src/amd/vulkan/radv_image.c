#include "radv_private.h"
#include "vk_format.h"
#include "radv_amdgpu_surface.h"
static unsigned
radv_choose_tiling(struct radv_device *Device,
		   const struct radv_image_create_info *create_info)
{
   const VkImageCreateInfo *pCreateInfo = create_info->vk_info;
   const struct vk_format_description *desc = vk_format_description(pCreateInfo->format);
   //   bool force_tiling = templ->flags & R600_RESOURCE_FLAG_FORCE_TILING;

   /* MSAA resources must be 2D tiled. */
   if (pCreateInfo->samples)
      return RADEON_SURF_MODE_2D;

   return RADEON_SURF_MODE_2D;
}
static int
radv_init_surface(struct radv_device *device,
		  struct radeon_surf *surface,
		  const struct radv_image_create_info *create_info)
{
   const VkImageCreateInfo *pCreateInfo = create_info->vk_info;
   unsigned array_mode = radv_choose_tiling(device, create_info);
   bool is_depth, is_stencil;

   surface->npix_x = pCreateInfo->extent.width;
   surface->npix_y = pCreateInfo->extent.height;
   surface->npix_z = pCreateInfo->extent.depth;

   surface->blk_w = vk_format_get_blockwidth(pCreateInfo->format);
   surface->blk_h = vk_format_get_blockheight(pCreateInfo->format);
   surface->blk_d = 1;
   surface->array_size = pCreateInfo->arrayLayers;
   surface->last_level = pCreateInfo->mipLevels;

   surface->bpe = vk_format_get_blocksize(pCreateInfo->format);

   surface->nsamples = 1;
   surface->flags = RADEON_SURF_SET(array_mode, MODE);

   switch (pCreateInfo->imageType){
   case VK_IMAGE_TYPE_1D:
      if (pCreateInfo->arrayLayers > 1)
	 surface->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_1D_ARRAY, TYPE);
      else
	 surface->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_1D, TYPE);
      break;
   case VK_IMAGE_TYPE_2D:
      if (pCreateInfo->arrayLayers > 1)
	 surface->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_2D_ARRAY, TYPE);
      else
	 surface->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_2D, TYPE);
      break;
   case VK_IMAGE_TYPE_3D:
      surface->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_3D, TYPE);
      break;
   }

   surface->flags |= RADEON_SURF_HAS_TILE_MODE_INDEX;
   return 0;
}
		  

VkResult
radv_image_create(VkDevice _device,
                 const struct radv_image_create_info *create_info,
                 const VkAllocationCallbacks* alloc,
                 VkImage *pImage)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   const VkImageCreateInfo *pCreateInfo = create_info->vk_info;
   struct radv_image *image = NULL;
   VkResult r;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

   radv_assert(pCreateInfo->mipLevels > 0);
   radv_assert(pCreateInfo->arrayLayers > 0);
   radv_assert(pCreateInfo->samples > 0);
   radv_assert(pCreateInfo->extent.width > 0);
   radv_assert(pCreateInfo->extent.height > 0);
   radv_assert(pCreateInfo->extent.depth > 0);

   image = radv_alloc2(&device->alloc, alloc, sizeof(*image), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!image)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   memset(image, 0, sizeof(*image));
   image->type = pCreateInfo->imageType;
   image->extent = pCreateInfo->extent;
   image->vk_format = pCreateInfo->format;
   image->levels = pCreateInfo->mipLevels;
   image->array_size = pCreateInfo->arrayLayers;
   image->samples = pCreateInfo->samples;
   image->tiling = pCreateInfo->tiling;

   radv_init_surface(device, &image->surface, create_info);

   radv_amdgpu_surface_init(device->addrlib, &image->surface);
   image->size = image->surface.bo_size;

   *pImage = radv_image_to_handle(image);

   return VK_SUCCESS;

fail:
   if (image)
      radv_free2(&device->alloc, alloc, image);

   return r;
}


VkResult
radv_CreateImage(VkDevice device,
                const VkImageCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkImage *pImage)
{
   return radv_image_create(device,
      &(struct radv_image_create_info) {
         .vk_info = pCreateInfo,
      },
      pAllocator,
      pImage);
}

void
radv_DestroyImage(VkDevice _device, VkImage _image,
                 const VkAllocationCallbacks *pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);

   radv_free2(&device->alloc, pAllocator, radv_image_from_handle(_image));
}

void radv_GetImageSubresourceLayout(
    VkDevice                                    device,
    VkImage                                     _image,
    const VkImageSubresource*                   pSubresource,
    VkSubresourceLayout*                        pLayout)
{
   RADV_FROM_HANDLE(radv_image, image, _image);

   pLayout->rowPitch = image->surface.level[0].pitch_bytes;
   pLayout->size = image->surface.bo_size;
}


VkResult
radv_CreateImageView(VkDevice _device,
                    const VkImageViewCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkImageView *pView)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   struct radv_image_view *view;

   view = radv_alloc2(&device->alloc, pAllocator, sizeof(*view), 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (view == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   //   radv_image_view_init(view, device, pCreateInfo, NULL, ~0);

   *pView = radv_image_view_to_handle(view);

   return VK_SUCCESS;
}

void
radv_DestroyImageView(VkDevice _device, VkImageView _iview,
                     const VkAllocationCallbacks *pAllocator)
{
   RADV_FROM_HANDLE(radv_device, device, _device);
   RADV_FROM_HANDLE(radv_image_view, iview, _iview);


   radv_free2(&device->alloc, pAllocator, iview);
}
