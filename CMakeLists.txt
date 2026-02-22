# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)

# 告诉编译器：把 spiffs_data 文件夹打包成 SPIFFS 镜像，并烧录到名为 storage 的分区里
spiffs_create_partition_image(storage spiffs_data FLASH_IN_PROJECT)
