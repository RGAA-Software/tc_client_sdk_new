find_package(websocketpp REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

find_package(libyuv CONFIG REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/tc_message)
include_directories(${CMAKE_SOURCE_DIR}/tc_3rdparty/eventbus/src)

set(SDK_SRCS
        thunder_sdk.cpp
        ffmpeg_video_decoder.cpp
        raw_image.cpp
        ws_client.cpp
)

add_library(tc_sdk ${SDK_SRCS})
target_link_libraries(tc_sdk websocketpp::websocketpp ${FFMPEG_LIBRARIES} Opus::opus yuv)
target_link_libraries(tc_sdk protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)