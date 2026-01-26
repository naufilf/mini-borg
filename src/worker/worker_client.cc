#include "src/worker/worker_client.h"

#include <iostream>
#include <memory>
#include <string>

using grpc::ClientContext;
using grpc::Status;

using mini_borg::FinishJobRequest;
using mini_borg::FinishJobResponse;
using mini_borg::HeartbeatRequest;
using mini_borg::HeartbeatResponse;

namespace mini_borg
{
    // constructor, use member initializer to be more efficient
    WorkerClient::WorkerClient(std::shared_ptr<grpc::Channel> channel) : stub_(Coordinator::NewStub(channel)) {};

    // use pointer to job vector since recieved_jobs can be null
    bool WorkerClient::SendHeartbeat(const std::string &worker_id, int cpu_cores, int ram_mb, std::vector<mini_borg::Job> *recieved_jobs)
    {
        HeartbeatRequest request;
        HeartbeatResponse response;
        ClientContext context;

        request.set_worker_id(worker_id);

        auto *resources = request.mutable_available_resources();
        resources->set_cpu_cores(cpu_cores);
        resources->set_memory_mb(ram_mb);

        // network call using these important arguments
        Status status = stub_->Heartbeat(&context, request, &response);

        if (status.ok())
        {
            if (recieved_jobs != nullptr)
            {
                recieved_jobs->clear();
                for (const auto &job : response.jobs_to_start())
                {
                    recieved_jobs->push_back(job);
                }
            }
            return true;
        }
        else
        {
            std::cout << status.error_message() << std::endl;
        }

        return false;
    }

    bool WorkerClient::NotifyJobFinished(const std::string &job_id,
                                         const std::string &worker_id,
                                         bool success,
                                         const mini_borg::Resource &released_resources)
    {
        FinishJobRequest request;
        FinishJobResponse response;
        ClientContext context;

        request.set_worker_id(worker_id);
        request.set_job_id(job_id);

        auto *res = request.mutable_released_resources();
        res->set_cpu_cores(released_resources.cpu_cores());
        res->set_memory_mb(released_resources.memory_mb());
        request.set_success(success);

        Status status = stub_->FinishJob(&context, request, &response);

        if (status.ok())
        {
            return true;
        }
        else
        {
            std::cerr << "[Worker] FinishJob RPC failed: " << status.error_message() << std::endl;
        }

        return false;
    }
}