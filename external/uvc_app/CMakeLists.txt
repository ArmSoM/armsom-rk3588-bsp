cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
PROJECT(uvc_app)

include_directories(uvc)

set(LIB_SOURCE
    uvc/uvc-gadget.c
    uvc/uvc_video.cpp
    uvc/yuv.c
    uvc/uvc_control.c
    uvc/uvc_encode.cpp
    uvc/mpi_enc.c
    uvc/uevent.c
    uvc/drm.c
)
add_library(rkuvc SHARED ${LIB_SOURCE})
target_link_libraries(rkuvc pthread drm rockchip_mpp)

set(SOURCE
    main.c
    ${LIB_SOURCE}
)

ADD_EXECUTABLE(uvc_app ${SOURCE})
target_link_libraries(uvc_app pthread drm rockchip_mpp)

set(CAMERA_SOURCE
    camera_uvc.c
    ${LIB_SOURCE}
)

ADD_EXECUTABLE(camera_uvc ${CAMERA_SOURCE})
target_link_libraries(camera_uvc rkisp rkisp_api pthread drm rockchip_mpp)

install(TARGETS rkuvc DESTINATION lib)
install(DIRECTORY ./uvc DESTINATION include
        FILES_MATCHING PATTERN "*.h")

install(TARGETS uvc_app DESTINATION bin)
install(DIRECTORY . DESTINATION bin
        FILES_MATCHING PATTERN "*.sh"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install(TARGETS camera_uvc DESTINATION bin)
