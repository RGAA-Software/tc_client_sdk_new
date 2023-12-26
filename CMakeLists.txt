find_package(websocketpp REQUIRED)

set(SDK_SRCS
        thunder_sdk.cpp
)

add_library(tc_sdk ${SDK_SRCS})
target_link_libraries(tc_sdk websocketpp::websocketpp)