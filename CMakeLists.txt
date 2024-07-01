#find_package(websocketpp REQUIRED)
find_package(protobuf CONFIG REQUIRED)
#find_package(Opus CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(libyuv CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

if(ANDROID)

else()
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGL OpenGLWidgets Multimedia)

    #C:\source\vcpkg\packages\ffmpeg_x64-windows-static\lib
    link_directories(${VCPKG_ROOT}/packages/ffmpeg_x64-windows-static/lib)
endif()

if(MSVC)
    add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT>)
    add_compile_options("/bigobj")
endif()

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/tc_message_new)
include_directories(${CMAKE_SOURCE_DIR}/tc_3rdparty/)
include_directories(${CMAKE_SOURCE_DIR}/tc_3rdparty/eventbus/src)
include_directories(${CMAKE_SOURCE_DIR}/tc_3rdparty/asio2/include)
include_directories(${CMAKE_SOURCE_DIR}/tc_3rdparty/asio2/3rd)

set(WEBRTC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../tc_webrtc_client/6167)
include_directories(${WEBRTC_ROOT}/)
include_directories(${WEBRTC_ROOT}/include)
include_directories(${WEBRTC_ROOT}/include/third_party/abseil-cpp)
include_directories(${WEBRTC_ROOT}/include/third_party/jsoncpp/source/include/)
include_directories(${WEBRTC_ROOT}/include/base)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SDK_SRCS
    thunder_sdk.cpp
    video_decoder.cpp
    ffmpeg_video_decoder.cpp
    mediacodec_video_decoder.cpp
    ws_client.cpp
    stream_helper.cpp
    webrtc_client.cpp
    cast_receiver.cpp
    app_timer.cpp
    statistics.cpp
)

set(GL_SRCS
    gl/shader_program.cpp
    gl/director.cpp
    gl/renderer.cpp
    gl/sprite.cpp
    gl/raw_image.cpp
)

add_library(tc_sdk ${SDK_SRCS} ${GL_SRCS})
target_link_libraries(tc_sdk PRIVATE ${FFMPEG_LIBRARIES} Opus::opus glm::glm)#websocketpp::websocketpp
target_link_libraries(tc_sdk PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
target_link_libraries(tc_sdk PRIVATE tc_opus_codec_new tc_message)

message("ffmpeg libraries: ${FFMPEG_LIBRARIES}")

if (ANDROID)
else ()
    target_link_libraries(tc_sdk PRIVATE
            Qt6::Widgets
            Qt6::Core
            Qt6::OpenGL
            Qt6::OpenGLWidgets
            Qt6::Multimedia
    )
endif ()