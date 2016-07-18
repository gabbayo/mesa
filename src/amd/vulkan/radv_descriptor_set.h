#pragma once

#include <vulkan/vulkan.h>

#define MAX_SETS         8

struct radv_descriptor_set_binding_layout {
   VkDescriptorType type;

   /* Number of array elements in this binding */
   uint16_t array_size;

   uint16_t offset;
   uint16_t buffer_offset;
   uint16_t dynamic_offset_offset;

   /* redundant with the type, each for a single array element */
   uint16_t size;
   uint16_t buffer_count;
   uint16_t dynamic_offset_count;

   /* Immutable samplers (or NULL if no immutable samplers) */
   struct radv_sampler **immutable_samplers;
};

struct radv_descriptor_set_layout {
   /* Number of bindings in this descriptor set */
   uint16_t binding_count;

   /* Total size of the descriptor set with room for all array entries */
   uint16_t size;

   /* Shader stages affected by this descriptor set */
   uint16_t shader_stages;

   /* Number of buffers in this descriptor set */
   uint16_t buffer_count;

   /* Number of dynamic offsets used by this descriptor set */
   uint16_t dynamic_offset_count;

   /* Bindings in this descriptor set */
   struct radv_descriptor_set_binding_layout binding[0];
};

struct radv_pipeline_layout {
   struct {
      struct radv_descriptor_set_layout *layout;
      uint32_t dynamic_offset_start;
   } set[MAX_SETS];

   uint32_t num_sets;

   struct {
      bool has_dynamic_offsets;
   } stage[MESA_SHADER_STAGES];
};
