load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "job_store_interface",
    hdrs = ["job_store.h"],
    deps = [
        "//src/proto:scheduler_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "postgres_job_store",
    srcs = ["postgres_job_store.cc"],
    hdrs = ["postgres_job_store.h"],
    deps = [
        ":job_store_interface",
    ],
    linkopts = ["-lpqxx", "-lpq"], 
    visibility = ["//visibility:public"],
)

cc_library(
    name = "coordinator_service",
    srcs = ["coordinator_service.cc"],
    hdrs = ["coordinator_service.h"],
    deps = [
        ":job_store_interface",
        "//src/proto:scheduler_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "master_server",
    srcs = ["master_server.cc"],
    deps = [
        ":coordinator_service",
        ":postgres_job_store",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_test(
    name = "coordinator_test",
    srcs = ["coordinator_test.cc", "mock_job_store.h"],
    deps = [":coordinator_service",
            ":job_store_interface", 
            "@googletest//:gtest_main", 
            "@googletest//:gtest",
           ],
)