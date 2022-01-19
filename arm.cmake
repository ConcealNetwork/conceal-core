include(TestCXXAcceptsFlag)
include(CheckCXXCompilerFlag)

# ARCH defines the target architecture, either by an explicit identifier or
# one of the following two keywords. By default, ARCH a value of 'native':
# target arch = host arch, binary is not portable. When ARCH is set to the
# string 'default', no -march arg is passed, which creates a binary that is
# portable across processors in the same family as host processor.  In cases
# when ARCH is not set to an explicit identifier, cmake's builtin is used
# to identify the target architecture, to direct logic in this cmake script.
# Since ARCH is a cached variable, it will not be set on first cmake invocation.
if (NOT ARCH_ID)
  if (NOT ARCH OR ARCH STREQUAL "" OR ARCH STREQUAL "native" OR ARCH STREQUAL "default")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "")
      set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
    endif()
    set(ARCH_ID "${CMAKE_SYSTEM_PROCESSOR}")
  else()
    set(ARCH_ID "${ARCH}")
  endif()
endif()

string(TOLOWER "${ARCH_ID}" ARM_ID)
string(SUBSTRING "${ARM_ID}" 0 3 ARM_TEST)

if (ARM_TEST STREQUAL "arm")
  set(ARM 1)
  string(SUBSTRING "${ARM_ID}" 0 5 ARM_TEST)
  if (ARM_TEST STREQUAL "armv7")
    set(ARM7 1)
  endif()
endif()

if (ARM_ID STREQUAL "aarch64" OR ARM_ID STREQUAL "arm64" OR ARM_ID STREQUAL "armv8-a")
  set(ARM 1)
  set(ARM8 1)
endif()

# Manual ARCH options for ARM
if(ARM7) # ARMv7 Pi 3/4 32Bit
  CHECK_CXX_ACCEPTS_FLAG("-march=armv7-a" TRY_ARCH)
  if(TRY_ARCH)
    message(STATUS "Setting march=armv7-a for ARMv7")
    set(ARCH_FLAG "-march=armv7-a")
  endif()
elseif(ARM8) # ARMv8 Pi 3/4 64Bit
  CHECK_CXX_ACCEPTS_FLAG("-march=armv8-a+fp+simd" TRY_ARCH)
  if(TRY_ARCH)
    message(STATUS "Setting -march=armv8-a+fp+simd for ARMv8")
    set(ARCH_FLAG "-march=armv8-a+fp+simd")
  endif()
endif()

# Check and set fpu and float settings
if(ARM)
  add_definitions(/DARM)
  message(STATUS "Setting FPU Flags for ARM Processors")
  if(NOT ARM8)
    # FPU
    CHECK_CXX_COMPILER_FLAG(-mfpu=neon-vfpv4 CXX_ACCEPTS_NEON_1)
    CHECK_CXX_COMPILER_FLAG(-mfpu=neon-fp-armv8 CXX_ACCEPTS_NEON_2)
    # FLOAT
    CHECK_CXX_ACCEPTS_FLAG(-mfloat-abi=hard CXX_ACCEPTS_MFLOAT_HARD)
  endif()
  if(ARM8)
    CHECK_CXX_ACCEPTS_FLAG(-mfix-cortex-a53-835769 CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
    CHECK_CXX_ACCEPTS_FLAG(-mfix-cortex-a53-843419 CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
  endif()

  if(ARM7)
    if(CXX_ACCEPTS_NEON_2)
      message(STATUS "Setting mfpu=neon-fp-armv8 for ARMv7")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon-fp-armv8")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon-fp-armv8")
    endif()
    if(CXX_ACCEPTS_NEON_1)
      message(STATUS "Setting mfpu=neon-vfpv4 for ARMv7")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon-vfpv4")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon-vfpv4")
    endif()
    if(CXX_ACCEPTS_MFLOAT_HARD)
      message(STATUS "Setting Hardware ABI for Floating Point")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard")
    endif()
  endif(ARM7)

  if(ARM8)
    if(CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
      message(STATUS "Enabling Cortex-A53 workaround 835769")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-835769")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-835769")
    endif()
    if(CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
      message(STATUS "Enabling Cortex-A53 workaround 843419")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-843419")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-843419")
    endif()
  endif(ARM8)
endif(ARM)