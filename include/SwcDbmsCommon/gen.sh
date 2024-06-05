rm -rf "Generated"
mkdir "Generated"
mkdir "Generated/go"
mkdir "Generated/cpp"

"Bin/protobuf/Linux/protoc" -I=Bin/protobuf/include -I=proto proto/Message/* --plugin=protoc-gen-go=Bin/protobuf/Linux/protoc-gen-go --plugin=protoc-gen-go-grpc=Bin/protobuf/Linux/protoc-gen-go-grpc  --go_out=Generated/go --go-grpc_out=Generated/go

"Bin/protobuf/Linux/protoc" -I=Bin/protobuf/include -I=proto proto/Service/* --plugin=protoc-gen-go=Bin/protobuf/Linux/protoc-gen-go --plugin=protoc-gen-go-grpc=Bin/protobuf/Linux/protoc-gen-go-grpc  --go_out=Generated/go --go-grpc_out=Generated/go

"Bin/protobuf/Linux/protoc" -I=Bin/protobuf/include -I=proto proto/Service/* --plugin=protoc-gen-grpc-gateway=Bin/protobuf/Linux/protoc-gen-grpc-gateway --grpc-gateway_out Generated/go --grpc-gateway_opt paths=source_relative --grpc-gateway_opt generate_unbound_methods=true

rm -rf "apiref"
mkdir "apiref"
mkdir "apiref/openapiv2"
"Bin/protobuf/Linux/protoc" -I=Bin/protobuf/include -I=proto proto/Service/* -I=Bin/protobuf/include/protoc-gen-openapiv2/options --plugin=protoc-gen-openapiv2=Bin/protobuf/Linux/protoc-gen-openapiv2 --openapiv2_out apiref/openapiv2 --openapiv2_opt generate_unbound_methods=true

mv "Generated/go/DBMS/SwcDbmsCommon/Generated/go/proto" "Generated/go/proto"
mv "Generated/go/Service/Service.pb.gw.go" "Generated/go/proto/service/Service.pb.gw.go"
rm -rf "Generated/go/DBMS"
rm -rf "Generated/go/Service"

"Bin/protobuf/Linux/protoc" -I=Bin/protobuf/include -I=proto proto/Message/* --plugin=protoc-gen-grpc=Bin/protobuf/Linux/grpc_cpp_plugin --cpp_out=Generated/cpp --grpc_out=Generated/cpp

"Bin/protobuf/Linux/protoc" -I=Bin/protobuf/include -I=proto proto/Service/* --plugin=protoc-gen-grpc=Bin/protobuf/Linux/grpc_cpp_plugin --cpp_out=Generated/cpp --grpc_out=Generated/cpp
