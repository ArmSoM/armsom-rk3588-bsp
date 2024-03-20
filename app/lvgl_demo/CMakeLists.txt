cmake_minimum_required(VERSION 3.8)

project(lvgl_demo)

add_definitions(-g -D__EXPORTED_HEADERS__)

include_directories(
	${PROJECT_SOURCE_DIR}/hal/
	${PROJECT_SOURCE_DIR}/lvgl/
	${PROJECT_SOURCE_DIR}/sys/
	${PROJECT_SOURCE_DIR}/cJSON/
	${CMAKE_SYSROOT}/usr/include/libdrm/
	${CMAKE_SYSROOT}/usr/include/lvgl/
	${CMAKE_SYSROOT}/usr/include/lvgl/lv_drivers/
	${CMAKE_SYSROOT}/usr/include/rockchip/
	)

if (LV_DRV_USE_SDL_GPU)
	add_definitions(-DUSE_SDL_GPU=1)
endif()
if (LV_DRV_USE_DRM)
	add_definitions(-DUSE_DRM=1)
endif()

aux_source_directory(${PROJECT_SOURCE_DIR}/hal SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/lvgl SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/sys SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/cJSON SRCS)

if (LV_USE_RK_DEMO)
	add_subdirectory(rk_demo)
else()
	add_subdirectory(lv_demo)
endif()

