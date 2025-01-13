param (
    [string]$ProtobufPath = "Bin/protobuf/Windows",
    [string]$gRPCPath = "Bin/protobuf/Windows"
)

function Remove-Directory {
    param (
        [string]$Path
    )
    
    if (Test-Path $Path) {
        Remove-Item -Recurse -Force -Path $Path
    }
}

Remove-Directory "Generated/go"
New-Item -Path "Generated/go" -ItemType Directory -Force

& "$ProtobufPath/protoc.exe" `
    -I=Bin/protobuf/include `
    -I=proto proto/Message/* `
    --plugin=protoc-gen-go=$ProtobufPath/protoc-gen-go.exe `
    --plugin=protoc-gen-go-grpc=$gRPCPath/protoc-gen-go-grpc.exe `
    --go_out=Generated/go `
    --go-grpc_out=Generated/go

& "$ProtobufPath/protoc.exe" `
    -I=Bin/protobuf/include `
    -I=proto proto/Service/* `
    --plugin=protoc-gen-go=$ProtobufPath/protoc-gen-go.exe `
    --plugin=protoc-gen-go-grpc=$gRPCPath/protoc-gen-go-grpc.exe `
    --go_out=Generated/go `
    --go-grpc_out=Generated/go

& "$ProtobufPath/protoc.exe" `
    -I=Bin/protobuf/include `
    -I=proto proto/Service/* `
    --plugin=protoc-gen-grpc-gateway=$gRPCPath/protoc-gen-grpc-gateway.exe `
    --grpc-gateway_out=Generated/go `
    --grpc-gateway_opt=paths=source_relative `
    --grpc-gateway_opt=generate_unbound_methods=true

Remove-Directory "apiref"
New-Item -Path "apiref/openapiv2" -ItemType Directory -Force

& "$ProtobufPath/protoc.exe" `
    -I=Bin/protobuf/include `
    -I=proto proto/Service/* `
    -I=Bin/protobuf/include/protoc-gen-openapiv2/options `
    --plugin=protoc-gen-openapiv2=$gRPCPath/protoc-gen-openapiv2.exe `
    --openapiv2_out=apiref/openapiv2 `
    --openapiv2_opt=generate_unbound_methods=true

Move-Item -Path "Generated/go/DBMS/SwcDbmsCommon/Generated/go/proto" -Destination "Generated/go/proto"
Move-Item -Path "Generated/go/Service/Service.pb.gw.go" -Destination "Generated/go/proto/service"

Remove-Directory "Generated/go/DBMS"
Remove-Directory "Generated/go/Service"