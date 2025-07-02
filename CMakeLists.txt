cmake_minimum_required(VERSION 3.26)

set(CMAKE_PREFIX_PATH "C:/Qt/6.8.2/msvc2022_64")
message(STATUS "Using CMAKE_PREFIX_PATH file: ${CMAKE_PREFIX_PATH}.")

#引入 vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/Users/KiraY/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake")
message(STATUS "Using CMAKE_TOOLCHAIN_FILE file: ${CMAKE_TOOLCHAIN_FILE}.")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(CollServerWithDBMS)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(jba/newmat11)

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Network
        REQUIRED)


include_directories(
        include
        include/hiredis
        include/qjson
        include/cxxopts
        include/httplib
        include/nlohmann_json
        include/octree
        include/lib
        basic_c_fun/
        neuron_editing/
        worm_straighten_c/
        neurontracing_vn2/
)

link_directories(
        lib
)

set(PROJECT_HEADERS
        analyze.h
        basic_c_fun/basic_surf_objs.h
        basic_c_fun/c_array_struct.hpp
        basic_c_fun/color_xyz.h
        basic_c_fun/v3d_basicdatatype.h
        basic_c_fun/v3d_message.h
        basic_c_fun/v_neuronswc.h
        coll_server.h
        collclient.h
        colldetection.h
        collthread.h
        neuron_editing/apo_xforms.h
        neuron_editing/global_feature_compute.h
        neuron_editing/neuron_format_converter.h
        neuron_editing/neuron_xforms.h
        neuron_editing/v_neuronswc.h
        sort_swc.h
        utils.h
        ./include/hiredis/hiredis.h
        include/cxxopts/cxxopts.hpp
        include/httplib/httplib.h
        include/nlohmann_json/json.hpp
        include/octree/octree.h
        include/octree/octree_container.h
        detect_crossing/ConfigManager.h
        detect_crossing/CrossingDetect.h
        detect_crossing/utilities.h
        detect_crossing/SwcReader.h
        detect_crossing/ResultWriter.h
        config/config.h
        neurontracing_vn2/common_value.h
        neurontracing_vn2/vn_imgpreprocess.h
        neurontracing_vn2/vn.h
        neurontracing_vn2/vn_app2.h
        neurontracing_vn2/vn_app1.h
        neurontracing_vn2/app1/v3dneuron_gd_tracing.h
        neurontracing_vn2/app1/gd.h
        neurontracing_vn2/app1/tip_detection.h
        neurontracing_vn2/app2/fastmarching_tree.h
        neurontracing_vn2/app2/hierarchy_prune.h
        neurontracing_vn2/app2/fastmarching_dt.h
        neurontracing_vn2/app2/marker_radius.h
)

set(PROJECT_SOURCES
        analyze.cpp
        basic_c_fun/basic_surf_objs.cpp
        basic_c_fun/v3d_message.cpp
        basic_c_fun/basic_4dimage_create.cpp
        coll_server.cpp
        collclient.cpp
        colldetection.cpp
        collthread.cpp
        main.cpp
        neuron_editing/apo_xforms.cpp
        neuron_editing/global_feature_compute.cpp
        neuron_editing/neuron_format_converter.cpp
        neuron_editing/neuron_xforms.cpp
        neuron_editing/v_neuronswc.cpp
        neuron_editing/neuron_sim_scores.cpp
        sort_swc.cpp
        utils.cpp
        detect_crossing/ConfigManager.cpp
        detect_crossing/CrossingDetect.cpp
        detect_crossing/ResultWriter.cpp
        config/config.cpp
        service/RpcCall.cpp
        service/RpcCall.h
        service/CachedProtoData.cpp
        service/CachedProtoData.h
        service/WrappedCall.cpp
        service/WrappedCall.h
        FileIo/AnoIo.hpp
        FileIo/ApoIo.hpp
        FileIo/SwcIo.hpp
        FileIo/FileIoInterface.hpp
        neurontracing_vn2/vn_imgpreprocess.cpp
        neurontracing_vn2/app2_connector.cpp
        neurontracing_vn2/app1_connector.cpp
        neurontracing_vn2/swc_convert.cpp
        neurontracing_vn2/app2/my_surf_objs.cpp
        neurontracing_vn2/app1/v3dneuron_gd_tracing.cpp
        neurontracing_vn2/app1/gd.cpp
        neurontracing_vn2/app1/tip_detection.cpp
        neurontracing_vn2/app1/calculate_cover_scores.cpp
        worm_straighten_c/bdb_minus.cpp
        worm_straighten_c/mst_prim_c.cpp
        worm_straighten_c/bfs_1root.cpp
        graph/dijk.cpp
)

# 自动查找所有 proto 文件
file(GLOB_RECURSE PROTO_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/SwcDbmsCommon/proto/*.proto
)
# 设置生成文件的输出目录
set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
# 获取 proto 文件的根目录路径
get_filename_component(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/SwcDbmsCommon/proto" ABSOLUTE)
# 用于收集生成的所有源文件和头文件
set(GENERATED_SRCS "")
set(GENERATED_HDRS "")

# output PROTO_FILES size
list(LENGTH PROTO_FILES PROTO_FILES_COUNT)
message(STATUS "Found ${PROTO_FILES_COUNT} [proto] files in SwcDbmsCommon/proto/")

# 对每个 proto 文件自动生成 C++ 代码
foreach (PROTO_FILE ${PROTO_FILES})
    message(STATUS "Processing proto file: ${PROTO_FILE}")
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    # 获取相对于proto根目录的路径结构
    file(RELATIVE_PATH PROTO_REL_PATH ${PROTO_PATH} ${PROTO_FILE})
    get_filename_component(PROTO_REL_DIR ${PROTO_REL_PATH} DIRECTORY)

    # 创建目标目录，保持原始结构
    set(OUTPUT_SUBDIR "${PROTO_SRC_DIR}/${PROTO_REL_DIR}")
    file(MAKE_DIRECTORY ${OUTPUT_SUBDIR})

    # 设置生成文件路径，包含子目录结构
    set(PROTO_SRC "${PROTO_SRC_DIR}/${PROTO_REL_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${PROTO_SRC_DIR}/${PROTO_REL_DIR}/${PROTO_NAME}.pb.h")

    # 生成 gRPC 服务文件
    set(GRPC_SRC "${PROTO_SRC_DIR}/${PROTO_REL_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HDR "${PROTO_SRC_DIR}/${PROTO_REL_DIR}/${PROTO_NAME}.grpc.pb.h")

    # 收集生成的文件
    list(APPEND GENERATED_SRCS ${PROTO_SRC} ${GRPC_SRC})
    list(APPEND GENERATED_HDRS ${PROTO_HDR} ${GRPC_HDR})

    # 执行代码生成命令
    add_custom_command(
            OUTPUT "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
            COMMAND $<TARGET_FILE:protobuf::protoc>
            ARGS --grpc_out=${PROTO_SRC_DIR}
            --cpp_out=${PROTO_SRC_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            -I${PROTO_PATH}
            ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating gRPC and Protobuf code from ${PROTO_FILE}"
            VERBATIM
    )
endforeach ()

# 添加自定义生成目标
add_custom_target(proto_gen DEPENDS ${GENERATED_SRCS} ${GENERATED_HDRS})

add_executable(${PROJECT_NAME} ${PROJECT_HEADERS} ${PROJECT_SOURCES} ${GENERATED_SRCS})

# 使主目标依赖于生成的代码
add_dependencies(${PROJECT_NAME} proto_gen)

# 包含生成的目录
target_include_directories(${PROJECT_NAME} PRIVATE ${PROTO_SRC_DIR})

if (WIN32)
    # 添加Windows特定的预处理器定义来避免头文件冲突
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _WIN32_WINNT=0x0A00
            WINVER=0x0A00
            STRICT
            VC_EXTRALEAN
            _CRT_SECURE_NO_WARNINGS
            _WINSOCK_DEPRECATED_NO_WARNINGS
            GTEST_HAS_PTHREAD=0
            _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
            # 防止Windows.h包含某些头文件
            NOGDICAPMASKS
            NOVIRTUALKEYCODES
            NOWINMESSAGES
            NOWINSTYLES
            NOSYSMETRICS
            NOMENUS
            NOICONS
            NOKEYSTATES
            NOSYSCOMMANDS
            NORASTEROPS
            NOSHOWWINDOW
            NOATOM
            NOCLIPBOARD
            NOCOLOR
            NOCTLMGR
            NODRAWTEXT
            NOKERNEL
            NOUSER
            NONLS
            NOMB
            NOMEMMGR
            NOMETAFILE
            NOMSG
            NOOPENFILE
            NOSCROLL
            NOSERVICE
            NOSOUND
            NOTEXTMETRIC
            NOWH
            NOWINOFFSETS
            NOCOMM
            NOKANJI
            NOHELP
            NOPROFILER
            NODEFERWINDOWPOS
            NOMCX
    )
    
    # 添加编译器特定选项来解决头文件冲突
    target_compile_options(${PROJECT_NAME} PRIVATE
            /wd4005  # 忽略宏重定义警告
            /wd4996  # 忽略过时函数警告 
            /bigobj  # 支持大对象文件
            /EHsc    # 异常处理模式
            /Zc:preprocessor  # 使用符合标准的预处理器
    )
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            libhiredis.dll.lib
            newmat11
            PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite
            PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    )
elseif (UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            libhiredis.so
            ${CMAKE_SOURCE_DIR}/include/lib/libv3dnewmat.a
            pthread
            PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite
            PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    )
endif()
