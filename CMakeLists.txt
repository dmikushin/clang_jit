cmake_minimum_required(VERSION 3.2)

project(clang_jit VERSION 1.0 LANGUAGES CXX)

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

find_package(LLVM CONFIG REQUIRED PATHS "/usr/lib/llvm-12/lib/cmake" NO_DEFAULT_PATH)
find_package(Clang CONFIG REQUIRED PATHS "/usr/lib/llvm-12/lib/cmake" NO_DEFAULT_PATH)

add_library(${PROJECT_NAME} SHARED
    src/ClangDriver.cpp
    include/ClangDriver.h)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14
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

target_link_libraries(${PROJECT_NAME} ${LLVM_LIBRARIES} clangTooling clangToolingCore clangDriver clangFrontend
    clangFrontendTool clangParse clangSema clangEdit clangLex clangAnalysis clangRewrite clangRewriteFrontend
    clangSerialization clangAST clangASTMatchers clangDynamicASTMatchers clangBasic clangIndex)

add_executable(test_${PROJECT_NAME}_1 src/test/test_${PROJECT_NAME}_1.cpp)
target_compile_definitions(test_${PROJECT_NAME}_1 PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(test_${PROJECT_NAME}_1 ${PROJECT_NAME})

add_executable(test_${PROJECT_NAME}_2 src/test/test_${PROJECT_NAME}_2.cpp)
target_compile_definitions(test_${PROJECT_NAME}_2 PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(test_${PROJECT_NAME}_2 ${PROJECT_NAME})

add_executable(test_${PROJECT_NAME}_3 src/test/test_${PROJECT_NAME}_3.cpp)
target_compile_definitions(test_${PROJECT_NAME}_3 PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(test_${PROJECT_NAME}_3 ${PROJECT_NAME})

