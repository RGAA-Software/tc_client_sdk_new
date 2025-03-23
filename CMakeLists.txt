#find_package(websocketpp REQUIRED)
#find_package(Opus CONFIG REQUIRED)
#find_package(libyuv CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
#find_package(Protobuf CONFIG REQUIRED)

if(ANDROID)
    find_package(FFMPEG REQUIRED)
else()
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGL OpenGLWidgets)
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

add_subdirectory(connection)

set(SDK_SRCS
    thunder_sdk.cpp
    video_decoder.cpp
    ffmpeg_video_decoder.cpp
    mediacodec_video_decoder.cpp
    net_client.cpp
    stream_helper.cpp
    cast_receiver.cpp
    app_timer.cpp
    statistics.cpp
    sdk_errors.cpp

    ${CONN_SRCS}
)

set(GL_SRCS
    gl/shader_program.cpp
    gl/director.cpp
    gl/renderer.cpp
    gl/sprite.cpp
    gl/raw_image.cpp
)

add_library(tc_sdk ${SDK_SRCS} ${GL_SRCS})
target_link_libraries(tc_sdk PRIVATE Opus::opus glm::glm)#websocketpp::websocketpp
target_link_libraries(tc_sdk PRIVATE tc_opus_codec_new tc_message)

message("ffmpeg libraries: ${FFMPEG_LIBRARIES}")

if (ANDROID)
    target_link_libraries(tc_sdk PRIVATE ${FFMPEG_LIBRARIES} tc_relay_client)
else ()
    target_link_libraries(tc_sdk PRIVATE libprotobuf tc_relay_client)

    set(FFMPEG_LIB_PATH ${CMAKE_SOURCE_DIR}/deps/tc_3rdparty/ffmpeg/lib/x64)
    link_directories(${FFMPEG_LIB_PATH})
    target_link_libraries(tc_sdk PRIVATE
            Qt6::Widgets Qt6::Core Qt6::OpenGL Qt6::OpenGLWidgets
            ${FFMPEG_LIB_PATH}/avcodec.lib ${FFMPEG_LIB_PATH}/avutil.lib ${FFMPEG_LIB_PATH}/avformat.lib
            ${FFMPEG_LIB_PATH}/swresample.lib ${FFMPEG_LIB_PATH}/swscale.lib ${FFMPEG_LIB_PATH}/avfilter.lib
    )
endif ()