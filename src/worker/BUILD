load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "worker_client",
    srcs = ["worker_client.cc"],
    hdrs = ["worker_client.h"],
    deps = [
        "//src/proto:scheduler_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_binary(
    name = "worker_client_bin",
    srcs = ["worker_main.cc"],
    deps = [
        ":worker_client",
        "@com_github_grpc_grpc//:grpc++",
    ],
)
