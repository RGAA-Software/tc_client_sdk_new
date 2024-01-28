find_package(websocketpp REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(libyuv CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

if(ANDROID)

else()
    add_definitions(-DPC_PLATFORM=1)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGL OpenGLWidgets Multimedia)
endif()

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/tc_message)
include_directories(${CMAKE_SOURCE_DIR}/tc_3rdparty/eventbus/src)

set(SDK_SRCS
        thunder_sdk.cpp
        video_decoder.cpp
        ffmpeg_video_decoder.cpp
        mediacodec_video_decoder.cpp
        ws_client.cpp
        stream_helper.cpp
)

set(GL_SRCS
        gl/shader_program.cpp
        gl/director.cpp
        gl/renderer.cpp
        gl/sprite.cpp
        gl/raw_image.cpp
)

add_library(tc_sdk ${SDK_SRCS} ${GL_SRCS})
target_link_libraries(tc_sdk PRIVATE websocketpp::websocketpp ${FFMPEG_LIBRARIES} Opus::opus yuv glm::glm)
target_link_libraries(tc_sdk PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)

if (ANDROID)

else ()
    target_link_libraries(tc_sdk PRIVATE Qt6::Widgets
            Qt6::Core
            Qt6::OpenGL
            Qt6::OpenGLWidgets
            Qt6::Multimedia
    )
endif ()