# libcdma

![Version](https://img.shields.io/badge/version-1.0.2-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

## Overview

libcdma is a user-space library that provides Crystal Direct Memory Access (CDMA) functionality for high-performance memory operations within the UnifiedBus (UB) ecosystem. It enables asynchronous one-sided DMA operations for direct memory access between hosts and host/devices, leveraging the UB protocol stack for efficient data transfer and resource management.

### UnifiedBus (UB) Technology

UnifiedBus (UB) is a super-node-oriented interconnect protocol designed to unify IO, memory access, and communication between processing units. The UB architecture provides:

- **High-performance data transfer** across processing nodes
- **Unified resource management** through entity-granularity allocation
- **Flexible resource composition** for diverse workload requirements
- **Efficient processing unit coordination** via standardized protocol stack

**Core UB Components:**
- **UBPU**: Processing unit supporting the UB protocol stack
- **UMMU**: Memory management unit for address mapping and permission validation
- **Entity**: Basic unit for device resource allocation, identified by EID (Entity Identifier)
- **UB Controller**: Executes the UB protocol stack operations

### CDMA's Role in UB

CDMA serves as the DMA engine within the UB system, providing:

- **Entity-granularity DMA resource allocation** for secure, isolated memory operations
- **UBMD-based memory addressing** using EID + TokenID + UBA (Unified Bus Address)
- **Asynchronous one-sided DMA operations** for direct memory access between hosts and host/devices
- **High-performance data movement** optimized for UB interconnect topology

### Key Features

- **One-sided host-to-host DMA**: Initiator-driven asynchronous memory read/write between hosts without remote participation
- **One-sided host-to-device DMA**: Direct memory access from host to device memory space
- **One-sided device-to-host DMA**: Direct memory access from device to host memory space
- **Atomic operations**: Compare-And-Swap (CAS) and Fetch-And-Add (FAA) for synchronization
- **Event-driven completion**: Polling and interrupt-based completion notification
- **Segment management**: Local segment registration and remote segment import for flexible memory access
- **Queue-based operation model**: Priority-based queue allocation with configurable depth and event modes

## Requirements

### Hardware Requirements

- **Processor Architecture**: HiSilicon processors (refer to hardware documentation for specific models)
- **DMA Hardware**: Crystal DMA (CDMA) hardware unit must be present and functional

### Kernel Requirements

- **Kernel Driver**: The cdma kernel driver must be loaded and operational
  - The driver provides the /dev/cdma device interface for user-space access
  - Driver must support CDMA ioctl commands defined in the kernel ABI
- **Kernel Headers**: Linux kernel headers must be installed for compilation
  - Required for cdma_abi.h which includes linux/types.h
  - Headers provide kernel-user space ABI definitions

### Software Dependencies

- **libummu**: Required external library for memory management utilities
  - Available as a separate package in openEuler
  - Linked dynamically at build time
- **CMake**: Version 3.5 or higher
  - Required for building the library from source
- **Standard C Library**: POSIX-compliant C library with standard headers

### Runtime Requirements

- **Kernel Module**: cdma driver must be loaded: modprobe cdma
- **Device Permissions**: User must have appropriate permissions to access /dev/cdma device nodes
- **Library Path**: libcdma.so must be accessible via system library path

## Directory Structure

```
libcdma/
├── include/               # Public API headers
│   └── cdma_u_lib.h       # Main library header
├── kernel_headers/        # Kernel ABI definitions  
│   └── cdma_abi.h         # Kernel-user space ABI
├── doc/                   # Documentation
│   └── CDMA_API.md        # Complete API reference
├── cdma_u_device.c        # Device discovery and management
├── cdma_u_device.h        
├── cdma_u_context.c       # Context creation/deletion
├── cdma_u_context.h       
├── cdma_u_jfs.c           # JFS (Jetty for Send) operations
├── cdma_u_jfs.h           
├── cdma_u_jfc.c           # JFC (Jetty for Completion) operations
├── cdma_u_jfc.h           
├── cdma_u_queue.c         # Queue management
├── cdma_u_queue.h         
├── cdma_u_segment.c       # Memory segment registration
├── cdma_u_segment.h       
├── cdma_u_event.c         # Event handling
├── cdma_u_event.h         
├── cdma_u_tp.c            # Transport point management
├── cdma_u_tp.h            
├── cdma_u_cmd.c           # Command processing
├── cdma_u_cmd.h           
├── cdma_u_db.c            # Doorbell operations
├── cdma_u_db.h            
├── cdma_u_common.c        # Common utilities
├── cdma_u_common.h        
├── cdma_u_log.c           # Logging functions
├── cdma_u_log.h           
├── cdma_u_types.h         # Type definitions
├── cdma_u_lib.c           # Library initialization
├── CMakeLists.txt         # Build configuration
├── LICENSE                # MIT License
└── README.md              # This file
```

## Header Files

[cdma_u_lib.h](./include/cdma_u_lib.h)

## Quick Start

After installing the library (from RPM package or source build), here is a minimal example:

```c
#include <stdio.h>
#include <cdma_u_lib.h>

int main(void) {
    uint32_t num_devices = 0;
    struct dma_device *dev_list = dma_get_device_list(&num_devices);
    
    if (!dev_list) {
        printf("No DMA devices found\n");
        return 1;
    }
    
    printf("Found %u DMA device(s)\n", num_devices);
    printf("First device: %s\n", dev_list[0].name);
    
    dma_free_device_list(dev_list, num_devices);
    return 0;
}
```


For complete API documentation, see [CDMA_API.md](./doc/CDMA_API.md).

## Note

Please refer to the CDMA library [API Doc](./doc/CDMA_API.md) for development.

## Support

For issues or questions, please contact:
- **Email**: <dev@openeuler.org>
- **Vendor Support**: Contact your hardware vendor's support channel for hardware-specific issues

## Contributing

We welcome contributions! Please follow openEuler contribution guidelines.

For detailed contribution guidelines, see the [openEuler Contribution Guide](https://www.openeuler.org/en/community/contribution/).

## License

This project is licensed under the MIT License.

- **SPDX-License-Identifier**: MIT
- **Copyright**: © 2025 HiSilicon Technologies Co., Ltd.

See the [LICENSE](./LICENSE) file for the full license text.
