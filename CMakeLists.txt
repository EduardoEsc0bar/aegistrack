cmake_minimum_required(VERSION 3.20)
project(sensor_fusion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(AEGISTRACK_REALTIME "Enable realtime allocation discipline and profiling hooks" OFF)
if(AEGISTRACK_REALTIME)
  add_compile_definitions(AEGISTRACK_REALTIME=1)
endif()
option(AEGISTRACK_ASAN "Enable AddressSanitizer" OFF)
option(AEGISTRACK_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if((AEGISTRACK_ASAN OR AEGISTRACK_UBSAN) AND NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set(AEGISTRACK_SANITIZER_FLAGS "")
  if(AEGISTRACK_ASAN)
    list(APPEND AEGISTRACK_SANITIZER_FLAGS -fsanitize=address)
  endif()
  if(AEGISTRACK_UBSAN)
    list(APPEND AEGISTRACK_SANITIZER_FLAGS -fsanitize=undefined)
  endif()
  add_compile_options(${AEGISTRACK_SANITIZER_FLAGS} -fno-omit-frame-pointer)
  add_link_options(${AEGISTRACK_SANITIZER_FLAGS} -fno-omit-frame-pointer)
endif()

option(AEGISTRACK_VIZ "Build SFML visualizer" ON)

if(AEGISTRACK_VIZ)
  # Homebrew ships SFML 3.x; prefer config package
  find_package(SFML 3 CONFIG QUIET COMPONENTS Graphics Window System)

  if(NOT SFML_FOUND)
    # Fallback for SFML 2.x
    find_package(SFML 2 CONFIG QUIET COMPONENTS graphics window system)
  endif()

  if(SFML_FOUND)
    message(STATUS "SFML found, visualization enabled")
    add_compile_definitions(AEGISTRACK_VIZ_ENABLED=1)
  else()
    message(STATUS "SFML not found, visualization disabled")
    set(AEGISTRACK_VIZ OFF)
  endif()
endif()
set(AEGISTRACK_VIZ_DEFAULT OFF)
if(SFML_FOUND)
  set(AEGISTRACK_VIZ_DEFAULT ON)
endif()
option(AEGISTRACK_VIZ "Enable optional SFML visualizer" ${AEGISTRACK_VIZ_DEFAULT})
if(NOT SFML_FOUND)
  message(STATUS "SFML not found, visualization disabled")
endif()
if(AEGISTRACK_VIZ AND NOT SFML_FOUND)
  message(STATUS "SFML not found, visualization disabled")
  set(AEGISTRACK_VIZ OFF CACHE BOOL "Enable optional SFML visualizer" FORCE)
endif()
if(AEGISTRACK_VIZ)
  add_compile_definitions(AEGISTRACK_VIZ=1)
endif()

include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
  doctest
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
)
FetchContent_MakeAvailable(doctest)

if(NOT DEFINED EIGEN_LOCAL_SOURCE)
  set(EIGEN_LOCAL_SOURCE "")
  if(EXISTS "/opt/homebrew/opt/eigen/include/eigen3/Eigen/Dense")
    set(EIGEN_LOCAL_SOURCE "/opt/homebrew/opt/eigen/include/eigen3")
  elseif(EXISTS "/usr/local/include/eigen3/Eigen/Dense")
    set(EIGEN_LOCAL_SOURCE "/usr/local/include/eigen3")
  elseif(EXISTS "/usr/include/eigen3/Eigen/Dense")
    set(EIGEN_LOCAL_SOURCE "/usr/include/eigen3")
  endif()
endif()

if(EIGEN_LOCAL_SOURCE)
  FetchContent_Declare(
    eigen
    SOURCE_DIR ${EIGEN_LOCAL_SOURCE}
  )
else()
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
endif()
FetchContent_MakeAvailable(eigen)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})

set(PROTO_GENERATED_SOURCES
  ${PROTO_BINARY_DIR}/common.pb.cc
  ${PROTO_BINARY_DIR}/detections.pb.cc
  ${PROTO_BINARY_DIR}/health.pb.cc
  ${PROTO_BINARY_DIR}/telemetry.pb.cc
  ${PROTO_BINARY_DIR}/health.grpc.pb.cc
  ${PROTO_BINARY_DIR}/telemetry.grpc.pb.cc
)
set(PROTO_GENERATED_HEADERS
  ${PROTO_BINARY_DIR}/common.pb.h
  ${PROTO_BINARY_DIR}/detections.pb.h
  ${PROTO_BINARY_DIR}/health.pb.h
  ${PROTO_BINARY_DIR}/telemetry.pb.h
  ${PROTO_BINARY_DIR}/health.grpc.pb.h
  ${PROTO_BINARY_DIR}/telemetry.grpc.pb.h
)

add_custom_command(
  OUTPUT ${PROTO_GENERATED_SOURCES} ${PROTO_GENERATED_HEADERS}
  COMMAND $<TARGET_FILE:protobuf::protoc>
  ARGS
    --cpp_out=${PROTO_BINARY_DIR}
    -I ${PROTO_SOURCE_DIR}
    ${PROTO_SOURCE_DIR}/common.proto
    ${PROTO_SOURCE_DIR}/detections.proto
    ${PROTO_SOURCE_DIR}/health.proto
    ${PROTO_SOURCE_DIR}/telemetry.proto
  COMMAND $<TARGET_FILE:protobuf::protoc>
  ARGS
    --grpc_out=${PROTO_BINARY_DIR}
    --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    -I ${PROTO_SOURCE_DIR}
    ${PROTO_SOURCE_DIR}/health.proto
    ${PROTO_SOURCE_DIR}/telemetry.proto
  DEPENDS
    ${PROTO_SOURCE_DIR}/common.proto
    ${PROTO_SOURCE_DIR}/detections.proto
    ${PROTO_SOURCE_DIR}/health.proto
    ${PROTO_SOURCE_DIR}/telemetry.proto
  VERBATIM
)

add_library(proto_lib
  ${PROTO_GENERATED_SOURCES}
  ${PROTO_GENERATED_HEADERS}
)
target_include_directories(proto_lib PUBLIC ${PROTO_BINARY_DIR})
target_link_libraries(proto_lib PUBLIC protobuf::libprotobuf gRPC::grpc++)

add_executable(run_sim
  src/agents/interceptor/interceptor.cpp
  src/agents/interceptor/interceptor_manager.cpp
  src/decision/assignment.cpp
  src/decision/blackboard.cpp
  src/decision/decision_engine.cpp
  src/decision/mission_behavior_tree.cpp
  src/decision/threat_scoring.cpp
  src/tools/run_sim.cpp
  src/common/fixed_rate_loop.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/tick_profiler.cpp
  src/common/time.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/measurement_buffer.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/observability/incident_capture.cpp
  src/observability/jsonl_logger.cpp
  src/observability/metrics.cpp
  src/scenario/truth_generator.cpp
  src/sensors/eoir/eoir_sensor.cpp
  src/sensors/radar/radar_sensor.cpp
  src/transport/message_bus.cpp
  src/viz/visualizer.cpp
)

if(AEGISTRACK_VIZ AND SFML_FOUND)
  if(TARGET SFML::Graphics)
    target_link_libraries(run_sim PRIVATE SFML::Graphics SFML::Window SFML::System)
  else()
    target_link_libraries(run_sim PRIVATE sfml-graphics sfml-window sfml-system)
  endif()
endif()

target_include_directories(run_sim PRIVATE src ${eigen_SOURCE_DIR})

add_executable(telemetry_ingest
  src/services/telemetry_ingest/main.cpp
  src/services/telemetry_ingest/conversion.cpp
  src/services/telemetry_ingest/ingest_pipeline.cpp
  src/services/telemetry_ingest/telemetry_ingest_service.cpp
  src/common/time.cpp
  src/observability/metrics.cpp
  src/transport/message_bus.cpp
)
target_include_directories(telemetry_ingest PRIVATE src ${eigen_SOURCE_DIR})
target_link_libraries(telemetry_ingest PRIVATE proto_lib)

add_executable(sensor_node
  src/services/sensor_node/main.cpp
  src/sensors/radar/radar_sensor.cpp
  src/common/fixed_rate_loop.cpp
  src/common/time.cpp
  src/observability/metrics.cpp
)
target_include_directories(sensor_node PRIVATE src ${eigen_SOURCE_DIR})
target_link_libraries(sensor_node PRIVATE proto_lib)

add_executable(run_cluster_demo
  src/tools/run_cluster_demo.cpp
  src/agents/interceptor/interceptor.cpp
  src/decision/blackboard.cpp
  src/decision/decision_engine.cpp
  src/decision/mission_behavior_tree.cpp
  src/services/telemetry_ingest/conversion.cpp
  src/services/telemetry_ingest/ingest_pipeline.cpp
  src/services/telemetry_ingest/telemetry_ingest_service.cpp
  src/common/fixed_rate_loop.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/tick_profiler.cpp
  src/common/time.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/measurement_buffer.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/observability/metrics.cpp
  src/transport/message_bus.cpp
  src/viz/visualizer.cpp
)
target_include_directories(run_cluster_demo PRIVATE src ${eigen_SOURCE_DIR})
target_link_libraries(run_cluster_demo PRIVATE proto_lib)

add_executable(run_load_test
  src/tools/run_load_test.cpp
  src/services/telemetry_ingest/conversion.cpp
  src/services/telemetry_ingest/ingest_pipeline.cpp
  src/common/fixed_rate_loop.cpp
  src/common/time.cpp
  src/observability/metrics.cpp
  src/transport/message_bus.cpp
)
target_include_directories(run_load_test PRIVATE src ${eigen_SOURCE_DIR})
target_link_libraries(run_load_test PRIVATE proto_lib)

add_executable(run_bench
  src/tools/run_bench.cpp
  src/tools/bench_harness.cpp
  src/tools/sim_config.cpp
  src/common/fixed_rate_loop.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/tick_profiler.cpp
  src/common/time.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/measurement_buffer.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/observability/metrics.cpp
  src/observability/metrics_csv.cpp
  src/scenario/truth_generator.cpp
  src/sensors/eoir/eoir_sensor.cpp
  src/sensors/radar/radar_sensor.cpp
)
target_include_directories(run_bench PRIVATE src ${eigen_SOURCE_DIR})

add_executable(run_profile
  src/tools/run_profile.cpp
  src/tools/bench_harness.cpp
  src/tools/sim_config.cpp
  src/common/fixed_rate_loop.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/tick_profiler.cpp
  src/common/time.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/measurement_buffer.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/observability/metrics.cpp
  src/scenario/truth_generator.cpp
  src/sensors/eoir/eoir_sensor.cpp
  src/sensors/radar/radar_sensor.cpp
)
target_include_directories(run_profile PRIVATE src ${eigen_SOURCE_DIR})

add_executable(run_replay
  src/tools/run_replay.cpp
  src/decision/blackboard.cpp
  src/decision/mission_behavior_tree.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/time.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/observability/jsonl_logger.cpp
  src/observability/metrics.cpp
)
target_include_directories(run_replay PRIVATE src ${eigen_SOURCE_DIR})

add_executable(run_incident_replay
  src/tools/run_incident_replay.cpp
  src/tools/incident_replay.cpp
  src/agents/interceptor/interceptor.cpp
  src/decision/blackboard.cpp
  src/decision/decision_engine.cpp
  src/decision/mission_behavior_tree.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/time.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/observability/jsonl_logger.cpp
  src/observability/metrics.cpp
)
target_include_directories(run_incident_replay PRIVATE src ${eigen_SOURCE_DIR})

add_executable(tests
  tests/test_assignment.cpp
  tests/test_blackboard.cpp
  tests/test_behavior_tree.cpp
  tests/test_conversion.cpp
  tests/test_health_stale.cpp
  tests/test_ingest_queue.cpp
  tests/test_interceptor.cpp
  tests/test_interceptor_manager.cpp
  tests/test_proto_roundtrip.cpp
  tests/test_sanity.cpp
  tests/test_common.cpp
  tests/test_covariance_sanity.cpp
  tests/test_bench_smoke.cpp
  tests/test_ekf_cv.cpp
  tests/test_ekf_skips_bad_measurement.cpp
  tests/test_eoir_update.cpp
  tests/test_eoir_sensor.cpp
  tests/test_hungarian.cpp
  tests/test_hungarian_association.cpp
  tests/test_gating_numerical_robustness.cpp
  tests/test_incident_replay.cpp
  tests/test_measurement_buffer.cpp
  tests/test_messages_and_bus.cpp
  tests/test_metrics_csv.cpp
  tests/test_mission_behavior_tree.cpp
  tests/test_radar_sensor.cpp
  tests/test_replay_determinism.cpp
  tests/test_realtime_reserves.cpp
  tests/test_scenario.cpp
  tests/test_track_manager.cpp
  tests/test_tick_profiler.cpp
  tests/test_viz_build.cpp
  tests/test_joseph_update_symmetry.cpp
  src/common/fixed_rate_loop.cpp
  src/common/covariance_checks.cpp
  src/common/math_checks.cpp
  src/common/tick_profiler.cpp
  src/common/time.cpp
  src/agents/interceptor/interceptor.cpp
  src/agents/interceptor/interceptor_manager.cpp
  src/decision/assignment.cpp
  src/decision/blackboard.cpp
  src/decision/decision_engine.cpp
  src/decision/mission_behavior_tree.cpp
  src/decision/threat_scoring.cpp
  src/services/telemetry_ingest/conversion.cpp
  src/services/telemetry_ingest/ingest_pipeline.cpp
  src/fusion_core/ekf_cv.cpp
  src/fusion_core/association/hungarian.cpp
  src/fusion_core/gating.cpp
  src/fusion_core/measurement_buffer.cpp
  src/fusion_core/track.cpp
  src/fusion_core/track_manager.cpp
  src/tools/incident_replay.cpp
  src/observability/incident_capture.cpp
  src/observability/jsonl_logger.cpp
  src/observability/metrics.cpp
  src/observability/metrics_csv.cpp
  src/scenario/truth_generator.cpp
  src/sensors/eoir/eoir_sensor.cpp
  src/sensors/radar/radar_sensor.cpp
  src/tools/bench_harness.cpp
  src/tools/sim_config.cpp
  src/transport/message_bus.cpp
  src/viz/visualizer.cpp
)
target_include_directories(tests PRIVATE src ${eigen_SOURCE_DIR})
target_link_libraries(tests PRIVATE doctest::doctest proto_lib)
add_test(NAME tests COMMAND tests)

if(AEGISTRACK_VIZ)
  target_include_directories(run_sim PRIVATE ${SFML_INCLUDE_DIRS})
  target_include_directories(run_cluster_demo PRIVATE ${SFML_INCLUDE_DIRS})
  target_include_directories(tests PRIVATE ${SFML_INCLUDE_DIRS})
  if(TARGET SFML::Graphics AND TARGET SFML::Window AND TARGET SFML::System)
    target_link_libraries(run_sim PRIVATE SFML::Graphics SFML::Window SFML::System)
    target_link_libraries(run_cluster_demo PRIVATE SFML::Graphics SFML::Window SFML::System)
    target_link_libraries(tests PRIVATE SFML::Graphics SFML::Window SFML::System)
  else()
    target_link_libraries(run_sim PRIVATE ${SFML_LIBRARIES})
    target_link_libraries(run_cluster_demo PRIVATE ${SFML_LIBRARIES})
    target_link_libraries(tests PRIVATE ${SFML_LIBRARIES})
  endif()
endif()
