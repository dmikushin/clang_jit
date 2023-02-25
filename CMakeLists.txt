cmake_minimum_required(VERSION 3.2)

project(JitFromScratch VERSION 1.0 LANGUAGES CXX)

if(WIN32)
    set(OS_NAME "Win")
    set(DISABLE_RTTI_FLAG /GR-)
elseif(APPLE)
    set(OS_NAME "OSX")
    set(DISABLE_RTTI_FLAG -fno-rtti)
elseif(UNIX)
    set(OS_NAME "Linux")
    set(DISABLE_RTTI_FLAG -fno-rtti)
else()
    message(FATAL_ERROR "Operating system not supported")
endif()

add_executable(JitFromScratch
    src/JitFromScratch.cpp
    src/ClangCC1Driver.cpp
    include/ClangCC1Driver.h
    include/ClangCC1Args_${OS_NAME}.h)

set_target_properties(JitFromScratch PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

# add LLVM libraries, set LLVM_* variables
find_package(LLVM CONFIG REQUIRED PATHS "/usr/lib/llvm-12/lib/cmake" NO_DEFAULT_PATH)
find_package(Clang CONFIG REQUIRED PATHS "/usr/lib/llvm-12/lib/cmake" NO_DEFAULT_PATH)

if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(JitFromScratch PRIVATE ${DISABLE_RTTI_FLAG})
endif()

if(USE_LLD)
	message(STATUS "Add option to link with LLD")
	target_link_libraries(JitFromScratch PRIVATE "-fuse-ld=lld")
endif()

target_include_directories(JitFromScratch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS})

# LLVM definitions
separate_arguments(LLVM_DEFINITIONS)
target_compile_definitions(JitFromScratch PRIVATE ${LLVM_DEFINITIONS})

# JitFromScratch dependencies
llvm_map_components_to_libnames(LLVM_LIBS
    core
    executionengine
    native
    object
    orcjit
    runtimedyld
    support)

target_link_libraries(JitFromScratch PRIVATE ${LLVM_LIBS})

# Project-specific definitions
target_compile_definitions(JitFromScratch
  PRIVATE
    # Supply path to Clang resources in the LLVM/Clang build directory
    JIT_FROM_SCRATCH_CLANG_RESOURCE_DIR=${LLVM_BUILD_BINARY_DIR}/lib/clang/${LLVM_PACKAGE_VERSION}
)

# Clang dependencies
target_link_libraries(JitFromScratch PRIVATE ${LLVM_LIBRARIES} clangTooling clangToolingCore clangDriver clangFrontend
    clangFrontendTool clangParse clangSema clangEdit clangLex clangAnalysis clangRewrite clangRewriteFrontend
    clangSerialization clangAST clangASTMatchers clangDynamicASTMatchers clangBasic clangIndex)

message("")
message("JitFromScratch")

get_target_property(ALL_INCLUDE_DIRECTORIES JitFromScratch INCLUDE_DIRECTORIES)
message(STATUS "Include directories: ${ALL_INCLUDE_DIRECTORIES}")

get_target_property(ALL_LINK_LIBRARIES JitFromScratch LINK_LIBRARIES)
message(STATUS "Link libraries: ${ALL_LINK_LIBRARIES}")

get_target_property(ALL_COMPILE_OPTIONS JitFromScratch COMPILE_OPTIONS)
message(STATUS "Compile options: ${ALL_COMPILE_OPTIONS}")

get_target_property(ALL_COMPILE_DEFINITIONS JitFromScratch COMPILE_DEFINITIONS)
message(STATUS "Compile definitions: ${ALL_COMPILE_DEFINITIONS}")

message(STATUS "Other flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Other flags Debug: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Other flags Release: ${CMAKE_CXX_FLAGS_RELEASE}")
