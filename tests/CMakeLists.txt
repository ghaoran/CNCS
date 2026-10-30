# Catch2 Unit Tests
#
# 说明：测试目标的权威定义在仓库根 CMakeLists.txt（BUILD_TESTS 选项）中，
# 因为它需要与 CNCS_Core 静态库共享 include、编译定义和依赖。
#
# 这个文件原先是一份独立且失效的第二套构建描述：
#   - 未被任何 add_subdirectory 引用；
#   - target_link_libraries(CNCS_tests PRIVATE ... CNCS) 把可执行文件当库链接，
#     CMake 在 configure 阶段直接报错；
#   - 还声明了并不存在的 include 目录（src/core/config 等）。
# 为避免再出现两套互相矛盾的规则，这里只做转发，不再自行定义目标。
#
# 单独构建测试的方法：
#   cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build
#   ctest --test-dir build --output-on-failure -C <Debug|Release>

if(NOT DEFINED PROJECT_NAME)
    message(FATAL_ERROR
        "tests/CMakeLists.txt 需要在根项目内使用。"
        "请在仓库根目录执行: cmake -S . -B build -DBUILD_TESTS=ON")
endif()

if(NOT TARGET CNCS_tests)
    message(FATAL_ERROR
        "未找到 CNCS_tests 目标：请在根目录配置时加上 -DBUILD_TESTS=ON。")
endif()
