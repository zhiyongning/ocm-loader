# 找到 ocm-access-manager-cpp
list(APPEND CMAKE_PREFIX_PATH "/Users/zhiyong/mydev/ocm-access-manager-cpp/build/final-install-image/lib/cmake")
find_package(here.ocm-access-manager-cpp REQUIRED)

# 定义库
add_library(ocmloader-core
    OcmMapEngine.cpp
    TileIDConverter.cpp
)

# 包含路径
target_include_directories(ocmloader-core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# 链接到 ocm-access-manager-cpp
target_link_libraries(ocmloader-core
    PUBLIC
    here::ocm-access-manager-cpp  # 注意这里是 imported target
)