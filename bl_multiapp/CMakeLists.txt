cmake_minimum_required(VERSION 3.16)

# 👉 QUAN TRỌNG:
# Ép CMake khi cấu hình subproject bootloader phải thêm đường dẫn components/bootloader của PROJECT,
# để nó "nhìn thấy" thư mục subproject/main bạn đã tạo.
# Dùng biến đúng tên EXTRA_COMPONENT_DIRS (không phải IDF_EXTRA_COMPONENT_DIRS).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/bootloader"
    CACHE STRING "" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bl_multiapp)
