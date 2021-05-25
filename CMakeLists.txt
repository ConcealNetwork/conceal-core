cmake_minimum_required(VERSION 2.8.6)

include(CheckCXXCompilerFlag)
set(VERSION "0.2")
# Packaged from main commits
set(COMMIT 72946d9)
set(REFS " (HEAD -> master)")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release CACHE STRING INTERNAL)
set(CMAKE_SKIP_INSTALL_RULES ON)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON)
set(CMAKE_SUPPRESS_REGENERATION ON)

project(concealX)

include_directories(include src external "${CMAKE_BINARY_DIR}/version")

if(APPLE OR FREEBSD)
  include_directories(SYSTEM /usr/include/malloc)
  if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
  endif()
  enable_language(ASM)
endif()

if(MSVC)
  include_directories(src/Platform/Windows)
elseif(APPLE)
  include_directories(src/Platform/OSX)
else()
	include_directories(src/Platform/Linux)
	include(TestCXXAcceptsFlag)
	include(CheckCCompilerFlag)
  if (NOT ARCH)
    set(ARCH native CACHE STRING "CPU to build for: -march value or 'default' to not pass -march at all")
  endif()
  message(STATUS "Building on ${CMAKE_SYSTEM_PROCESSOR}")
  if(ARCH STREQUAL "default")
    set(ARCH_FLAG "")
  else()
    set(ARCH_FLAG "-march=${ARCH}")
    if(ARCH STREQUAL "native")
      check_c_compiler_flag(-march=native CC_SUPPORTS_MARCH_NATIVE)
      if (NOT CC_SUPPORTS_MARCH_NATIVE)
        check_c_compiler_flag(-mtune=native CC_SUPPORTS_MTUNE_NATIVE)
        if (CC_SUPPORTS_MTUNE_NATIVE)
          set(ARCH_FLAG "-mtune=${ARCH}")
        else()
          set(ARCH_FLAG "")
        endif()
      endif()
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(ARCH_FLAG "-march=x86-64")
      endif()
    endif()
	endif()
endif()

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

# Check support for AES
option(NO_AES "Explicitly disable AES support" ${NO_AES})

if(NO_AES) # No AES
	message(STATUS "AES support explicitly disabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_AES")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_AES")

elseif(NOT ARM) # x86/64
	message(STATUS "AES support enabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -maes")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -maes")

endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAG}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_FLAG}")

# Check and set fpu and float settings
if(ARM)
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

set(STATIC ${MSVC} CACHE BOOL "Link libraries statically")

if(MSVC)
  add_definitions("/bigobj /MP /W3 /GS- /D_CRT_SECURE_NO_WARNINGS /wd4996 /wd4345 /D_WIN32_WINNT=0x0600 /DWIN32_LEAN_AND_MEAN /DGTEST_HAS_TR1_TUPLE=0 /D_VARIADIC_MAX=8 /D__SSE4_1__")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10485760")
  if(STATIC)
    foreach(VAR CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE)
      string(REPLACE "/MD" "/MT" ${VAR} "${${VAR}}")
    endforeach()
  endif()
  include_directories(SYSTEM src/platform/msc)
  add_definitions(/DHAVE_ROTR)
else()
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # This option has no effect in glibc version less than 2.20.
    # Since glibc 2.20 _BSD_SOURCE is deprecated, this macro is recomended instead
    add_definitions("-D_DEFAULT_SOURCE -D_GNU_SOURCE")
	endif()

  set(WARNINGS "-Wall -Wextra -Wpointer-arith -Wundef -Wvla -Wwrite-strings -Wno-error=extra -Wno-error=unused-function -Wno-error=deprecated-declarations -Wno-error=sign-compare -Wno-error=strict-aliasing -Wno-error=type-limits -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=undef -Wno-error=uninitialized -Wno-error=unused-result")
  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(WARNINGS "${WARNINGS} -Wno-error=mismatched-tags -Wno-error=null-conversion -Wno-overloaded-shift-op-parentheses -Wno-error=shift-count-overflow -Wno-error=tautological-constant-out-of-range-compare -Wno-error=unused-private-field -Wno-error=unneeded-internal-declaration -Wno-error=unused-function")
    add_definitions(/DHAVE_ROTR)
  else()
    set(WARNINGS "${WARNINGS} -Wlogical-op -Wno-error=maybe-uninitialized -Wno-error=clobbered -Wno-error=unused-but-set-variable")
  endif()
  if(MINGW)
    set(WARNINGS "${WARNINGS} -Wno-error=unused-value")
    set(MINGW_FLAG "-DWIN32_LEAN_AND_MEAN")
    include_directories(SYSTEM src/platform/mingw)
  else()
    set(MINGW_FLAG "")
  endif()
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 5.1))
    set(WARNINGS "${WARNINGS} -Wno-error=odr")
    add_definitions(/DHAVE_ROTR)
  endif()
  set(C_WARNINGS "-Waggregate-return -Wnested-externs -Wold-style-definition -Wstrict-prototypes")
  set(CXX_WARNINGS "-Wno-reorder -Wno-missing-field-initializers")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 ${MINGW_FLAG} ${WARNINGS} ${C_WARNINGS}")
  if(NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${MINGW_FLAG} ${WARNINGS} ${CXX_WARNINGS}")
  if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGTEST_HAS_TR1_TUPLE=0")
  endif()
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 4.8))
    set(DEBUG_FLAGS "-g3 -Og")
  else()
    set(DEBUG_FLAGS "-g3 -O0")
  endif()
  set(RELEASE_FLAGS "-g3 -O0")

endif()

if(APPLE)
	add_definitions(/DHAVE_ROTR)
endif()

if(ARM)
	add_definitions(/DARM)
endif()

if(STATIC)
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_USE_STATIC_RUNTIME ON)
endif()

#set(Boost_DEBUG on)
find_package(Boost 1.55 REQUIRED COMPONENTS system filesystem thread date_time chrono regex serialization program_options)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

if(MINGW)
  set(Boost_LIBRARIES "${Boost_LIBRARIES};ws2_32;mswsock")
elseif(APPLE)
  set(Boost_LIBRARIES "${Boost_LIBRARIES}")
elseif(NOT MSVC)
  set(Boost_LIBRARIES "${Boost_LIBRARIES};rt")
endif()

# option(BUILD_TESTS "Build tests." ON)

if(BUILD_TESTS)
  add_subdirectory(tests)
  enable_testing()
endif()

set(COMMIT_ID_IN_VERSION ON CACHE BOOL "Include commit ID in version")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/version")
if (NOT COMMIT_ID_IN_VERSION)
  set(VERSION "${VERSION}-unknown")
  configure_file("src/version.h.in" "version/version.h")
  add_custom_target(version ALL)
elseif(DEFINED COMMIT)
  string(REPLACE "." "\\." VERSION_RE "${VERSION}")
  if(NOT REFS MATCHES "(\\(|, )tag: v${VERSION_RE}(\\)|, )")
    set(VERSION "${VERSION}-g${COMMIT}")
  endif()
  configure_file("src/version.h.in" "version/version.h")
  add_custom_target(version ALL)
else()
  find_package(Git QUIET)
  if(Git_FOUND OR GIT_FOUND)
    message(STATUS "Found Git: ${GIT_EXECUTABLE}")
    add_custom_target(version ALL "${CMAKE_COMMAND}" "-D" "VERSION=${VERSION}" "-D" "GIT=${GIT_EXECUTABLE}" "-D" "TO=${CMAKE_BINARY_DIR}/version/version.h" "-P" "src/version.cmake" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  else()
    message(STATUS "WARNING: Git was not found!")
    set(VERSION "${VERSION}-unknown")
    configure_file("src/version.h.in" "version/version.h")
    add_custom_target(version ALL)
  endif()
endif()

add_subdirectory(external)
add_subdirectory(src)