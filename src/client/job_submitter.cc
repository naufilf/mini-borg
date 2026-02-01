#include <grpcpp/grpcpp.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "src/proto/scheduler.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using mini_borg::Coordinator;
using mini_borg::Resource;
using mini_borg::SubmitJobRequest;
using mini_borg::SubmitJobResponse;

// Helper to parse flags
std::string GetFlag(int argc, char** argv, const std::string& flag, const std::string& default_val) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find(flag + "=") == 0) {
            return arg.substr(flag.length() + 1);
        }
    }
    return default_val;
}

int main(int argc, char** argv) {
    // Parse args
    std::string target_str = GetFlag(argc, argv, "--target", "localhost:50051");
    std::string job_name = GetFlag(argc, argv, "--name", "test-job");
    int cpu_req = std::atoi(GetFlag(argc, argv, "--cpu", "1").c_str());
    int ram_req = std::atoi(GetFlag(argc, argv, "--ram", "512").c_str());

    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());

    std::unique_ptr<Coordinator::Stub> stub = Coordinator::NewStub(channel);

    SubmitJobRequest request;
    request.set_name(job_name);
    SubmitJobResponse response;
    ClientContext context;

    auto* inner_resources = request.mutable_resource_reqs();
    inner_resources->set_cpu_cores(cpu_req);
    inner_resources->set_memory_mb(ram_req);
    std::cout << "[Client] Submitting job..." << std::endl;

    Status status = stub->SubmitJob(&context, request, &response);

    if (status.ok()) {
        std::cout << "[Client] SUCCESS! Job ID: " << response.job_id() << std::endl;
    } else {
        std::cout << "[Client] FAILED: " << status.error_message() << std::endl;
    }

    return 0;
}