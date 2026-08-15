cmake_minimum_required (VERSION 3.5...4.3)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.3" CACHE STRING "Minimum macOS deployment target" FORCE)
project (psicore)
set(CMAKE_BUILD_TYPE Release)
set(CONAN_DISABLE_CHECK_COMPILER true)
#set(CMAKE_BUILD_TYPE Debug)

set(PSI_ARCH "macos_arm64")

# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# C++17 is required by metal-cpp.
add_compile_options(-Wall -std=c++17 -Wno-deprecated)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -ffast-math -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-Og")

# Metal backend: vendored metal-cpp headers and the system frameworks.
set(PSI_METAL_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ext/metal-cpp")
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)

message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")

# Yeah setting this to anything will not affect actually the output directory.
# At least on macOS.
set(PSI_CORE_OUTPUT_DIR lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PSI_CORE_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PSI_CORE_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PSI_CORE_OUTPUT_DIR})

message("CMAKE_LIBRARY_OUTPUT_DIRECTORY is ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message("CMAKE_ARCHIVE_OUTPUT_DIRECTORY is ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

set(SOURCES
	src/ext/stb_image_impl.cpp
	src/ext/qoi_impl.cpp
	src/ext/miniaudio_impl.cpp
	src/ext/tiny_gltf_loader.cpp
	src/ext/fpng.cpp
	src/ext/metal_cpp_impl.cpp
	src/PSIMetalContext.cpp
	# Objective-C++: the only AppKit/CAMetalLayer glue in the engine.
	src/PSIMetalLayer.mm
	src/PSIAudio.cpp
	src/PSIGlobals.cpp 
	src/PSILog.cpp 
	src/PSIFileUtils.cpp
	src/PSIMath.cpp 
	src/PSIVideo.cpp 
	src/PSIGeometry.cpp 
	src/PSIResourceManager.cpp 
	src/PSIGLUtils.cpp 
	src/PSIGLShader.cpp 
	src/PSIGLTransform.cpp 
	src/PSIGLTexture.cpp 
	src/PSIAABB.cpp
	src/PSIRenderObj.cpp 
	src/PSIRenderMesh.cpp
	src/PSIFrameTimer.cpp
	src/PSICycler.cpp 
	src/PSIScaler.cpp 
	src/PSIColor.cpp 
	src/PSIString.cpp 
	src/PSICamera.cpp 
	src/PSIGLMesh.cpp 
	src/PSIGLRenderer.cpp 
	src/PSIQuadGeometry.cpp 
	src/PSIIcosahedronGeometry.cpp 
	src/PSIGeometryData.cpp 
	src/PSIRenderScene.cpp 
	src/PSICubeGeometry.cpp 
	src/PSITetrahedronGeometry.cpp 
	src/PSICuboidGeometry.cpp 
	src/PSIPlaneGeometry.cpp 
	src/PSITextRenderer.cpp
	src/PSITimeDisplay.cpp
	src/PSIGLTFLoader.cpp 
	src/PSIPrismGeometry.cpp 
)

set(HEADER_FILES
	src/PSIMetal.h
	src/PSIAudio.h
	src/PSIGlobals.h 
	src/PSILog.h 
	src/PSIFileUtils.h
	src/PSIMath.h 
	src/PSIVideo.h 
	src/PSIGeometry.h 
	src/PSIResourceManager.h 
	src/PSIGLUtils.h 
	src/PSIGLShader.h 
	src/PSIGLTransform.h 
	src/PSIGLMaterial.h 
	src/PSIGLTexture.h 
	src/PSIAudio.h
	src/PSIAABB.h
	src/PSIRenderObj.h 
	src/PSIRenderMesh.h
	src/PSIRenderContext.h
	src/PSIFrameTimer.h
	src/PSICycler.h 
	src/PSIScaler.h 
	src/PSIColor.h 
	src/PSIString.h 
	src/PSICamera.h 
	src/PSIGLMesh.h 
	src/PSIGLRenderer.h 
	src/PSIQuadGeometry.h 
	src/PSIIcosahedronGeometry.h 
	src/PSIGeometryData.h 
	src/PSIRenderScene.h 
	src/PSICubeGeometry.h 
	src/PSICuboidGeometry.h 
	src/PSIPlaneGeometry.h 
	src/PSILight.h
	src/PSITextRenderer.h
	src/PSITimeDisplay.h
	src/PSIGLTFLoader.h 
	src/PSIPrismGeometry.h
)
	
add_library(${PROJECT_NAME} STATIC ${SOURCES})

# metal-cpp headers are included as <Metal/Metal.hpp>, so the vendor root goes on
# the include path. PUBLIC so psiengine picks it up through the core target too.
target_include_directories(${PROJECT_NAME} PUBLIC ${PSI_METAL_CPP_DIR})

target_link_libraries(${PROJECT_NAME}
	${CONAN_LIBS}
	${METAL_FRAMEWORK}
	${QUARTZCORE_FRAMEWORK}
	${FOUNDATION_FRAMEWORK}
	${APPKIT_FRAMEWORK})

install (TARGETS ${PROJECT_NAME} 
	ARCHIVE DESTINATION ${PSI_CORE_OUTPUT_DIR}
	LIBRARY DESTINATION ${PSI_CORE_OUTPUT_DIR})
