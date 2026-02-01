#ifndef MINI_BORG_MASTER_COORDINATOR_SERVICE_H_
#define MINI_BORG_MASTER_COORDINATOR_SERVICE_H_

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <thread>
#include <vector>

#include "src/master/job_store.h"
#include "src/proto/scheduler.grpc.pb.h"

namespace mini_borg {
    // Data structure so service can keep track of nodes
    struct WorkerNode {
        std::string id;
        int cpu_cores;
        unsigned long long ram_mb;
        std::chrono::steady_clock::time_point last_heartbeat;
    };

    // Creates class that inherits the empty functions from generated Service class from gRPC proto, final means no
    // further inheritance
    class CoordinatorServiceImpl final : public Coordinator::Service {
    public:
        explicit CoordinatorServiceImpl(std::unique_ptr<JobStore> store);
        ~CoordinatorServiceImpl() override;

        grpc::Status SubmitJob(grpc::ServerContext* context, const SubmitJobRequest* request,
                               SubmitJobResponse* response) override;

        grpc::Status Heartbeat(grpc::ServerContext* context, const HeartbeatRequest* request,
                               HeartbeatResponse* response) override;

        grpc::Status FinishJob(grpc::ServerContext* context, const mini_borg::FinishJobRequest* request,
                               mini_borg::FinishJobResponse* response);

    private:
        void SaveJobToDb(const std::string& id, const std::string& name, const Resource& res,
                         const std::string& worker_id);
        int GetJobStatusFromDB(const std::string& job_id);
        void UpdateJobStatus(const std::string& job_id, int status_enum);
        void CheckDeadWorkers();
        int job_counter_ = 0;
        std::map<std::string, std::vector<mini_borg::Job>> pending_jobs_map_;
        std::thread reaper_thread_;
        std::atomic<bool> running_{true};
        std::unique_ptr<JobStore> store_;
        std::mutex counter_mutex_;
        std::map<std::string, WorkerNode> worker_map_;
        std::mutex map_mutex_;
    };

}  // namespace mini_borg

#endif  // MINI_BORG_MASTER_COORDINATOR_SERVICE_H_