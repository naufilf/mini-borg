#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "src/proto/scheduler.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using mini_borg::Coordinator;
using mini_borg::Resource;
using mini_borg::SubmitJobRequest;
using mini_borg::SubmitJobResponse;

int main(int argc, char **argv)
{
    std::string target_str = "localhost:50051";
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());

    std::unique_ptr<Coordinator::Stub> stub = Coordinator::NewStub(channel);

    SubmitJobRequest request;
    SubmitJobResponse response;
    ClientContext context;

    auto *inner_resources = request.mutable_resource_reqs();
    inner_resources->set_cpu_cores(4);
    inner_resources->set_memory_mb(8000);
    std::cout << "[Client] Submitting job..." << std::endl;

    Status status = stub->SubmitJob(&context, request, &response);

    if (status.ok())
    {
        std::cout << "[Client] SUCCESS! Job ID: " << response.job_id() << std::endl;
    }
    else
    {
        std::cout << "[Client] FAILED: " << status.error_message() << std::endl;
    }

    return 0;
}