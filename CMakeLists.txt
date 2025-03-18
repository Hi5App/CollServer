cmake_minimum_required(VERSION 3.26)

#引入 vcpkg
set(CMAKE_TOOLCHAIN_FILE "/home/seu/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(CollServerWithDBMS)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)


find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(CMAKE_PREFIX_PATH "C:/Qt/6.8.0/msvc2022_64")

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
        include/SwcDbmsCommon/Generated/cpp
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
        basic_c_fun/basic_4dimage.h
        coll_server.h
        collclient.h
        colldetection.h
        collthread.h
        neuron_editing/apo_xforms.h
        neuron_editing/global_feature_compute.h
        neuron_editing/neuron_format_converter.h
        neuron_editing/neuron_xforms.h
        neuron_editing/v_neuronswc.h
        neuron_annotator/utility/ImageLoaderBasic.h
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
        basic_c_fun/basic_4dimage.cpp
        basic_c_fun/stackutil.cpp
        basic_c_fun/mg_utilities.cpp
        basic_c_fun/mg_image_lib.cpp
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
        neuron_annotator/utility/ImageLoaderBasic.cpp
        sort_swc.cpp
        utils.cpp
        detect_crossing/ConfigManager.cpp
        detect_crossing/CrossingDetect.cpp
        detect_crossing/ResultWriter.cpp
        config/config.cpp
        include/SwcDbmsCommon/Generated/cpp/Message/Message.grpc.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Message/Message.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Message/Request.grpc.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Message/Request.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Message/Response.grpc.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Message/Response.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Service/Service.grpc.pb.cc
        include/SwcDbmsCommon/Generated/cpp/Service/Service.pb.cc
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
#        neurontracing_vn2/vaa3dneuron2_plugin.cpp
        neurontracing_vn2/swc_convert.cpp
        neurontracing_vn2/app2/my_surf_objs.cpp
        neurontracing_vn2/app1/v3dneuron_gd_tracing.cpp
        neurontracing_vn2/app1/gd.cpp
        neurontracing_vn2/app1/calculate_cover_scores.cpp
        worm_straighten_c/bdb_minus.cpp
        worm_straighten_c/mst_prim_c.cpp
        worm_straighten_c/bfs_1root.cpp
        graph/dijk.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_HEADERS} ${PROJECT_SOURCES})

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            libhiredis.dll.lib
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
            ${CMAKE_SOURCE_DIR}/include/lib_ubuntu/libtiff.so.5
            ${CMAKE_SOURCE_DIR}/include/lib_ubuntu/libv3dfftw3f.a
            ${CMAKE_SOURCE_DIR}/include/lib_ubuntu/libv3dtiff.a
            ${CMAKE_SOURCE_DIR}/include/lib_ubuntu/libv3dtiff.so
            pthread
            PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite
            PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    )
elseif (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            libhiredis.a
            pthread
            PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite
            PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    )
endif()
