cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 11)
add_definitions(-fno-rtti)
PROJECT(uac_app)

include_directories(src/include)

option(UAC_GRAPH "uac open graph" OFF)
option(UAC_MPI   "uac open mpi" ON)

if (${UAC_GRAPH})
    add_definitions(-DUAC_GRAPH)
    set(SOURCE_FILES_GRAPH
        src/graph/graph_control.cpp
        src/graph/uac_control_graph.cpp
    )
    message(STATUS "Build With Rockit Graph")
else()
    message(STATUS "Build None Rockit Graph")
endif()

if (${UAC_MPI})
    add_definitions(-DUAC_MPI)
    set(SOURCE_FILES_MPI
        src/mpi/uac_control_mpi.cpp
        src/mpi_common/mpi_control_common.cpp
    )
    message(STATUS "Build With Rockit Mpi")
else()
    message(STATUS "Build None Rockit Mpi")
endif()

set(LIB_SOURCE
    src/uevent.cpp
    src/uac_control.cpp
    src/uac_common_def.cpp
    src/uac_control_factory.cpp
    ${SOURCE_FILES_GRAPH}
    ${SOURCE_FILES_MPI}
)

add_library(rkuac SHARED ${LIB_SOURCE})
target_link_libraries(rkuac pthread)

set(SOURCE
    src/main.cpp
    ${LIB_SOURCE}
)

set(UAC_APP_DEPENDENT_LIBS
    pthread
    rockit
)

#set(UAC_AUDIO_ALGORITHM
#    ${CMAKE_CURRENT_LIST_DIR}/libs/libaec_bf_process.so)
#install(FILES ${UAC_AUDIO_ALGORITHM} DESTINATION lib)

ADD_EXECUTABLE(uac_app ${SOURCE})
target_link_libraries(uac_app ${UAC_APP_DEPENDENT_LIBS})

install(TARGETS rkuac DESTINATION lib)
install(DIRECTORY ./uac DESTINATION include
        FILES_MATCHING PATTERN "*.h")

option(ENABLE_DEMO_BOARD  "use demo board conf" OFF)
if (${ENABLE_DEMO_BOARD})
    install(DIRECTORY configs/demo/ DESTINATION share/uac_app FILES_MATCHING PATTERN "*.json")
else()
    install(DIRECTORY configs/ DESTINATION share/uac_app FILES_MATCHING PATTERN "configs_skv.json")
endif()

install(TARGETS uac_app DESTINATION bin)


if (NOT UAC_BUILDROOT)
    install(DIRECTORY . DESTINATION bin
    FILES_MATCHING PATTERN "*.sh"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_WRITE WORLD_EXECUTE)
endif()
