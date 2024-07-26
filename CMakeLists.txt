cmake_minimum_required(VERSION 3.5)

project(clang_jit VERSION 1.0 LANGUAGES C CXX)

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

if (TRUE)
find_package(LLVM CONFIG REQUIRED)
find_package(Clang CONFIG REQUIRED)
else()
# For Ubuntu with multiple versions of LLVM installed
find_package(LLVM CONFIG REQUIRED PATHS "/usr/lib/llvm-17/lib/cmake" NO_DEFAULT_PATH)
find_package(Clang CONFIG REQUIRED PATHS "/usr/lib/llvm-17/lib/cmake" NO_DEFAULT_PATH)
endif()

add_library(${PROJECT_NAME} SHARED
    src/ClangDriver.cpp
    include/ClangDriver.h)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(${PROJECT_NAME} PRIVATE ${DISABLE_RTTI_FLAG})
endif()

if(USE_LLD)
	message(STATUS "Add option to link with LLD")
	target_link_libraries(${PROJECT_NAME} PRIVATE "-fuse-ld=lld")
endif()

separate_arguments(LLVM_DEFINITIONS)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(${PROJECT_NAME} libclang clang-cpp LLVM)

add_executable(test_${PROJECT_NAME}_1 src/test/test_${PROJECT_NAME}_1.cpp)
target_compile_definitions(test_${PROJECT_NAME}_1 PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(test_${PROJECT_NAME}_1 ${PROJECT_NAME})

add_executable(test_${PROJECT_NAME}_2 src/test/test_${PROJECT_NAME}_2.cpp)
target_compile_definitions(test_${PROJECT_NAME}_2 PRIVATE ${LLVM_DEFINITIONS})
# Note: in order for the JIT dynamic library to use the symbols from the current process,
# those symbols must be present in the dynamic table. If the symbols are defined in the
# executable image, they won't be accessible to JIT, because the dynamic table is not
# generated for an executable by default. The solution is to link the executable with
# --export-dynamic, or set_target_properties(target PROPERTIES ENABLE_EXPORTS ON) for CMake
set_target_properties(test_${PROJECT_NAME}_2 PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(test_${PROJECT_NAME}_2 ${PROJECT_NAME})

add_executable(test_${PROJECT_NAME}_3 src/test/test_${PROJECT_NAME}_3.cpp)
target_compile_definitions(test_${PROJECT_NAME}_3 PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(test_${PROJECT_NAME}_3 PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(test_${PROJECT_NAME}_3 ${PROJECT_NAME})

