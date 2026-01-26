load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "submit_job",
    srcs = ["job_submitter.cc"],
    deps = [
        "//src/proto:scheduler_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)