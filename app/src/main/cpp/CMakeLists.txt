cmake_minimum_required(VERSION 3.22.1)
project(apexcine_camera_os LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(APEXCINE_CORE_SOURCES
    src/l0_runtime/ArenaAllocator.cpp
    src/l0_runtime/EdfScheduler.cpp
    src/l0_runtime/FrameClockAndTelemetry.cpp
    src/lp_platform/DeviceCapabilityProbe.cpp
    src/lp_platform/DeviceQuirkDatabase.cpp
    src/lp_platform/ThermalAndAdpfManager.cpp
    src/lp_platform/StorageBenchmarkAndIoProber.cpp
    src/l1_capture/NdkCameraCaptureEngine.cpp
    src/l2_pipeline/RawUnpackAndPlanarize.cpp
    src/l2_pipeline/SensorCalibrationAndClarity.cpp
    src/l2_pipeline/ComputationalRawMerge.cpp
    src/l2_pipeline/LosslessBayerCodec.cpp
    src/l3_container/ApexRawContainer.cpp
    src/l4_color/AcesColorPipeline.cpp
)

add_library(apexcine_core STATIC ${APEXCINE_CORE_SOURCES})
target_include_directories(apexcine_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(apexcine_core PRIVATE -O3 -Wall -Wextra -Wpedantic)
target_link_libraries(apexcine_core PUBLIC ${CMAKE_DL_LIBS})

if(ANDROID)
    add_library(apexcine_camera_os SHARED
        src/jni/ApexCineCameraOsJni.cpp
    )
    target_include_directories(apexcine_camera_os PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_options(apexcine_camera_os PRIVATE -O3 -Wall -Wextra)
    # Mandatory 16 KB ELF segment alignment for Android 15/16 (API 35/36+)
    target_link_options(apexcine_camera_os PRIVATE "-Wl,-z,max-page-size=16384")
    target_link_libraries(apexcine_camera_os
        PRIVATE
        apexcine_core
        camera2ndk
        mediandk
        android
        log
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(apexcine_core PUBLIC Threads::Threads)

    option(BUILD_TESTING "Build host C++20 unit tests and KPI benchmarks" ON)
    if(BUILD_TESTING)
        enable_testing()

        add_executable(apexcine_unit_tests
            tests/CameraOsUnitTests.cpp
        )
        target_compile_options(apexcine_unit_tests PRIVATE -O3 -Wall -Wextra)
        target_link_libraries(apexcine_unit_tests PRIVATE apexcine_core Threads::Threads)
        add_test(NAME ApexCineCameraOsUnitTests COMMAND apexcine_unit_tests)

        add_executable(apexcine_kpi_benchmark
            tests/CameraOsKpiBenchmark.cpp
        )
        target_compile_options(apexcine_kpi_benchmark PRIVATE -O3 -Wall -Wextra)
        target_link_libraries(apexcine_kpi_benchmark PRIVATE apexcine_core Threads::Threads)
        add_test(NAME ApexCineCameraOsKpiBenchmark COMMAND apexcine_kpi_benchmark)
    endif()
endif()
