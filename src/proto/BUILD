load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

proto_library(
    name = "scheduler_proto",
    srcs = ["scheduler.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "scheduler_cc_proto",
    deps = [":scheduler_proto"],
    visibility = ["//visibility:public"],
)

cc_grpc_library(
    name = "scheduler_cc_grpc",
    srcs = [":scheduler_proto"], # grpc library cant read raw text files, needs a package/library not just the proto file
    deps = [":scheduler_cc_proto"], # needs C++ messages to know what SubmitJobRequest is
    grpc_only = True, # tells compiler to only generate service logic, not messages which have already been generated in proto_library
    visibility = ["//visibility:public"],
)