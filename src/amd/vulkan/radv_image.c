#include "radv_private.h"

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
