cmake_minimum_required(VERSION 3.12.0)
project(propd-tools)

# To ease installation of utility files such as bash completions, etc.
# See: https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html
include(GNUInstallDirs)

enable_language(C CXX)
include(CheckSymbolExists)

# Use C11 with GNU extensions.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use C++20 with GNU extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DARWIN_C_SOURCE -D__DARWIN_C_LEVEL=__DARWIN_C_FULL")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DARWIN_C_SOURCE -D__DARWIN_C_LEVEL=__DARWIN_C_FULL")
endif()

# Different versions of clang require a different set of flags for -ftrivial-auto-var-init
# Simplify this contruct once old clang version support is dropped
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-ftrivial-auto-var-init=zero" COMPILER_SUPPORTS_TRIVIAL_ZERO_INIT)
if(COMPILER_SUPPORTS_TRIVIAL_ZERO_INIT)
	set(VAR_ZERO_INIT_FLAGS "-ftrivial-auto-var-init=zero")
else()
	check_cxx_compiler_flag("-ftrivial-auto-var-init=zero -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang"
				COMPILER_REQUIRES_ENABLE_TRIVIAL_ZERO_INIT)
	if(COMPILER_REQUIRES_ENABLE_TRIVIAL_ZERO_INIT)
		set(VAR_ZERO_INIT_FLAGS "-ftrivial-auto-var-init=zero -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang")
	endif()
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${VAR_ZERO_INIT_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${VAR_ZERO_INIT_FLAGS}")

# Android seems to use various attributes supported by clang but not by
# GCC which causes it to emit lots of warnings. Since these attributes
# don't seem to effect runtime behaviour simply disable the warnings.
add_compile_options(-Wno-attributes)
add_compile_options(-fPIC)

# libfsmgr (required by fastboot) requires a 64-bit off_t for lseek. On
# 32-bit glibc platforms this is not the case by default.
add_compile_definitions(_FILE_OFFSET_BITS=64 _LARGEFILE64_SOURCE)

check_symbol_exists(strlcpy "string.h" HAVE_STRLCPY)
check_symbol_exists(reallocarray "stdlib.h" HAVE_REALLOCARRAY)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include(CMakeLists.sysprop.txt)

# Targets which should be installed by `make install`.
install(TARGETS
	propd
	toolbox
	DESTINATION bin)
