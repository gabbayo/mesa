#pragma once

#include <amdgpu.h>
enum chip_family {
   CHIP_TAHITI,
   CHIP_PITCAIRN,
   CHIP_VERDE,
   CHIP_OLAND,
   CHIP_HAINAN,
   CHIP_BONAIRE,
   CHIP_KAVERI,
   CHIP_KABINI,
   CHIP_HAWAII,
   CHIP_MULLINS,
   CHIP_TONGA,
   CHIP_ICELAND,
   CHIP_CARRIZO,
   CHIP_FIJI,
   CHIP_STONEY,
   CHIP_POLARIS10,
   CHIP_POLARIS11,
};

enum chip_class {
   UNKNOWN = 0,
   SI = 1,
   CIK,
   VI
};

struct radv_device_info {

   uint32_t pci_id;
   enum chip_family family;
   enum chip_class chip_class;

};
