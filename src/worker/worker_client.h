#ifndef MINI_BORG_WORKER_WORKER_CLIENT_H_
#define MINI_BORG_WORKER_WORKER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "src/proto/scheduler.grpc.pb.h"

namespace mini_borg
{

    class WorkerClient
    {
    public:
        // use explicit to organize better, forces WorkerClient nameofworker(channel) creation
        explicit WorkerClient(std::shared_ptr<grpc::Channel> channel); // channel handles connection between stub and server

        bool SendHeartbeat(const std::string &worker_id, int cpu_cores, int ram_mb, std::vector<mini_borg::Job> *recieved_jobs); // pass primitives by value, objects by ref

        bool NotifyJobFinished(const std::string &job_id,
                               const std::string &worker_id,
                               bool success,
                               const mini_borg::Resource &released_resources);

    private:
        std::unique_ptr<Coordinator::Stub> stub_; // lets us easily communicate with the server
    };
}

#endif