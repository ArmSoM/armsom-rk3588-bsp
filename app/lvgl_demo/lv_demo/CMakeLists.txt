cmake_minimum_required(VERSION 3.8)

project(lv_demo)

include_directories(
	${PROJECT_SOURCE_DIR}/./
	)

if (LV_USE_DEMO_WIDGETS)
	add_definitions(-DLV_USE_DEMO_WIDGETS=1)
elseif (LV_USE_DEMO_KEYPAD_AND_ENCODER)
	add_definitions(-DLV_USE_DEMO_KEYPAD_AND_ENCODER=1)
elseif (LV_USE_DEMO_BENCHMARK)
	add_definitions(-DLV_USE_DEMO_BENCHMARK=1)
elseif (LV_USE_DEMO_STRESS)
	add_definitions(-DLV_USE_DEMO_STRESS=1)
elseif (LV_USE_DEMO_MUSIC)
	add_definitions(-DLV_USE_DEMO_MUSIC=1)
endif()

if (LV_DRV_USE_SDL_GPU)
	add_definitions(-DUSE_SDL_GPU=1)
endif()
if (LV_DRV_USE_DRM)
	add_definitions(-DUSE_DRM=1)
endif()

aux_source_directory(. SRCS)
aux_source_directory(./hal SRCS)
aux_source_directory(./lvgl SRCS)
aux_source_directory(./sys SRCS)

add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME}
	lvgl pthread m lv_drivers freetype lvgl_demos
)

if (LV_DRV_USE_SDL_GPU)
	target_link_libraries(${PROJECT_NAME} SDL2)
endif()
if (LV_DRV_USE_DRM)
	target_link_libraries(${PROJECT_NAME} drm)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(PROGRAMS S00-lv_demo DESTINATION /etc/init.d)

