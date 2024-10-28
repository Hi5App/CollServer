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

Remove-Directory "Generated/cpp"
New-Item -Path "Generated/cpp" -ItemType Directory -Force

& "$ProtobufPath/protoc.exe" `
    -I=Bin/protobuf/include `
    -I=proto proto/Message/* `
    --plugin=protoc-gen-grpc=$gRPCPath/grpc_cpp_plugin.exe `
    --cpp_out=Generated/cpp `
    --grpc_out=Generated/cpp

& "$ProtobufPath/protoc.exe" `
    -I=Bin/protobuf/include `
    -I=proto proto/Service/* `
    --plugin=protoc-gen-grpc=$gRPCPath/grpc_cpp_plugin.exe `
    --cpp_out=Generated/cpp `
    --grpc_out=Generated/cpp
