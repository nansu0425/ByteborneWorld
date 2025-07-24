# Find and link libraries
find_package(Protobuf CONFIG REQUIRED)

# .proto files directory
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILES
    "${PROTO_DIR}/Chat.proto"
)

# Generate C++ source files from .proto files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Add source to this static library.
add_library(Protocol STATIC
    ${PROTO_HDRS} ${PROTO_SRCS}
    "Type.h"
    "Queue.h" "Queue.cpp"
    "Factory.h"
)

# Link libraries
target_link_libraries(Protocol PUBLIC
    protobuf::libprotobuf
)

# Set include directories
target_include_directories(Protocol PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated headers
)
