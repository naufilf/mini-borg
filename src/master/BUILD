load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "coordinator_service",
    srcs = ["coordinator_service.cc"],
    hdrs = ["coordinator_service.h"],
    # ADD THIS LINE: Link against the Postgres C++ libraries
    linkopts = ["-lpqxx", "-lpq"], 
    deps = [
        "//src/proto:scheduler_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_binary(
    name = "master_server",
    srcs = ["master_server.cc"],
    deps = [
        ":coordinator_service",
        "@com_github_grpc_grpc//:grpc++",
    ],
)