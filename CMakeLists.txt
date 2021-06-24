project (psicore)
#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE Debug)

cmake_minimum_required (VERSION 3.2)
set(PSI_ARCH "macos_x64")

# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_compile_options(-Wall -std=c++14 -fno-exceptions)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -ffast-math")
set(CMAKE_CXX_FLAGS_DEBUG "-Og")

message("CMAKE_CXX_FLAGS is ${CMAKE_CXX_FLAGS}")
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
	src/ext/miniaudio_impl.cpp
	src/ext/tiny_gltf_loader.cpp
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
	src/PSIUniformMap.cpp 
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

set(HEADER_FILES, 
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
	src/PSITimer.h
	src/PSIFrameTimer.h
	src/PSICycler.h 
	src/PSIScaler.h 
	src/PSIColor.h 
	src/PSIString.h 
	src/PSICamera.h 
	src/PSIUniformMap.h 
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
target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS})

install (TARGETS ${PROJECT_NAME} 
	ARCHIVE DESTINATION ${PSI_CORE_OUTPUT_DIR}
	LIBRARY DESTINATION ${PSI_CORE_OUTPUT_DIR})
