load("@rules_java//java:defs.bzl", "java_binary")

java_binary(
    name = "gateway_application",
    srcs = glob(["*.java"]), 
    main_class = "gateway.GatewayApplication", 
    deps = [
        "@maven//:org_springframework_boot_spring_boot",
        "@maven//:org_springframework_boot_spring_boot_autoconfigure",
        "@maven//:org_springframework_boot_spring_boot_starter_web",
        "@maven//:org_springframework_spring_web",
        "//src/proto:scheduler_java_proto",
        "//src/proto:scheduler_java_grpc",
        "@io_grpc_grpc_java//api",
        "@io_grpc_grpc_java//netty",
    ],
)