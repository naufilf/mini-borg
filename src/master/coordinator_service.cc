#include "src/master/coordinator_service.h"

#include <uuid/uuid.h>

#include <iostream>
#include <pqxx/pqxx>

namespace mini_borg {

    using grpc::ServerContext;
    using grpc::Status;

    // initialize DB connection immediately
    CoordinatorServiceImpl::CoordinatorServiceImpl(std::unique_ptr<JobStore> store) : store_(std::move(store)) {
        ReconcileState();
        running_ = true;
        // this thread now constantly runs check dead workers
        reaper_thread_ = std::thread(&CoordinatorServiceImpl::CheckDeadWorkers, this);
    }

    CoordinatorServiceImpl::~CoordinatorServiceImpl() {
        running_ = false;
        if (reaper_thread_.joinable()) {
            reaper_thread_.join();
        }
    }

    void CoordinatorServiceImpl::ReconcileState() {
        std::cout << "[Master] --------------------------------------" << std::endl;
        std::cout << "[Master] Starting State Reconciliation..." << std::endl;

        // 1 represents JOB_STATUS_QUEUED
        std::vector<mini_borg::Job> recovered_jobs = store_->GetJobsOfStatusFromDB(1);

        if (recovered_jobs.empty()) {
            std::cout << "[Master] No queued jobs found in DB. Clean slate." << std::endl;
            return;
        }

        std::lock_guard<std::mutex> lock(map_mutex_);

        for (const auto& job : recovered_jobs) {
            std::string assigned_worker = job.worker_id();

            if (!assigned_worker.empty()) {
                pending_jobs_map_[assigned_worker].push_back(job);

                // Server keeps track of reseerved resources for queued jobs
                // No need to recalculate reserved resources for running jobs since resouce allocation handled in
                // SubmitJob
                reserved_resources_[assigned_worker].set_cpu_cores(reserved_resources_[assigned_worker].cpu_cores() +
                                                                   job.resource_reqs().cpu_cores());
                reserved_resources_[assigned_worker].set_memory_mb(reserved_resources_[assigned_worker].memory_mb() +
                                                                   job.resource_reqs().memory_mb());

                std::cout << "[Recovery] RESERVED: Job " << job.id() << " (" << job.resource_reqs().cpu_cores()
                          << " CPU) and (" << job.resource_reqs().memory_mb() << " RAM) for " << assigned_worker
                          << std::endl;
            }
        }

        std::cout << "[Master] Reconciliation Complete. Waiting for workers to reconnect." << std::endl;
        std::cout << "[Master] --------------------------------------" << std::endl;
    }

    void CoordinatorServiceImpl::CheckDeadWorkers() {
        try {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(5));

                std::lock_guard<std::mutex> lock(map_mutex_);
                std::cout << "[Reaper] Scanning for dead workers..." << std::endl;

                auto now = std::chrono::steady_clock::now();

                for (auto it = worker_map_.begin(); it != worker_map_.end();) {
                    auto last_seen = it->second.last_heartbeat;
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_seen).count();

                    std::cout << "Worker: " << it->first << " | CPU: " << it->second.cpu_cores
                              << " | RAM: " << it->second.ram_mb << " | Silence: " << duration << "s" << std::endl;

                    if (duration > 10) {  // if the worker silent for this long, its dead
                        std::cout << "[Master] Worker " << it->first << " is DEAD. Removing now..." << std::endl;

                        // TODO: find a way to optimize db calls before erasure of RUNNING jobs from DB for a dead worker
                        std::vector<mini_borg::Job> orphaned_jobs = store_->RequeueOrphanedJobs(it->first);
                        
                        for (const auto& job: orphaned_jobs) {
                            unassigned_queue_.push_back(job);
                        }

                        std::cout << "[Master] Requeued jobs running on worker " << it->first << std::endl;

                        it = worker_map_.erase(it);
                    } else {
                        it++;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[Master] Reaper thread error: " << e.what() << std::endl;
        }
    }

    Status CoordinatorServiceImpl::FinishJob(grpc::ServerContext* context, const mini_borg::FinishJobRequest* request,
                                             mini_borg::FinishJobResponse* response) {
        // ONLY save to db if job hasnt been cancelled, this is quick solution
        int database_status = store_->GetJobStatusFromDB(request->job_id());

        if (database_status == mini_borg::JOB_STATUS_COMPLETED || database_status == mini_borg::JOB_STATUS_FAILED) {
            std::cout << "[Master] Job " << request->job_id() << " already finished. Ignoring duplicate RPC."
                      << std::endl;
            return Status::OK;
        }

        if (database_status != mini_borg::JOB_STATUS_CANCELLED) {
            // check if job was a success
            int status = request->success() ? mini_borg::JOB_STATUS_COMPLETED : mini_borg::JOB_STATUS_FAILED;
            store_->UpdateJobStatus(request->job_id(), status);
        } else {
            std::cout << "[Master] Ignoring FinishJob for cancelled job " << request->job_id() << std::endl;
            return Status::OK;
        }

        auto it = worker_map_.find(request->worker_id());
        // reallocate resources
        if (it != worker_map_.end()) {
            it->second.cpu_cores += request->released_resources().cpu_cores();
            it->second.ram_mb += request->released_resources().memory_mb();

            std::cout << "[Master] Reclaimed resources from " << request->worker_id() << std::endl;
        } else {
            // Edge Case: Worker was reaped while job was running.
            std::cout << "[Master] Zombie Worker " << request->worker_id() << " detected." << std::endl;
        }

        return Status::OK;
    }

    Status CoordinatorServiceImpl::SubmitJob(grpc::ServerContext* context, const mini_borg::SubmitJobRequest* request,
                                             mini_borg::SubmitJobResponse* response) {
        // assign job id
        std::string job_id;
        uuid_t b_uuid;
        uuid_generate_random(b_uuid);
        char uuid_str[37];
        uuid_unparse_lower(b_uuid, uuid_str);

        job_id = "job-" + std::string(uuid_str);

        std::string assigned_worker_id;

        // logic for assigning job to worker
        {
            std::lock_guard<std::mutex> map_lock(map_mutex_);

            if (worker_map_.empty()) {
                return Status(grpc::StatusCode::UNAVAILABLE, "No workers detected");
            }

            for (auto& pair : worker_map_) {
                auto& worker = pair.second;
                if (worker.cpu_cores >= request->resource_reqs().cpu_cores() &&
                    worker.ram_mb >= request->resource_reqs().memory_mb()) {
                    mini_borg::Job new_job;
                    assigned_worker_id = worker.id;

                    new_job.set_id(job_id);
                    new_job.set_name(request->name());
                    new_job.set_status(mini_borg::JOB_STATUS_QUEUED);
                    // need to set JOB_STATUS_QUEUED before deducting resources so we never get into a situation during
                    // state reconcilation where resource aren't accounted for
                    new_job.set_worker_id(assigned_worker_id);
                    // deduct resources
                    worker.cpu_cores -= request->resource_reqs().cpu_cores();
                    worker.ram_mb -= request->resource_reqs().memory_mb();
                    *new_job.mutable_resource_reqs() = request->resource_reqs();

                    std::cout << "[Master] Now, " << assigned_worker_id << " has " << worker.cpu_cores << " cores and "
                              << worker.ram_mb << " MB of memory available." << std::endl;

                    pending_jobs_map_[assigned_worker_id].push_back(new_job);

                    break;
                }
            }
        }

        if (assigned_worker_id.empty()) {
            return Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "No worker has enough resources");
        }

        try {
            // save job to db logic
            store_->SaveJobToDb(job_id, request->name(), request->resource_reqs(), assigned_worker_id);
        } catch (const std::exception& e) {
            std::cerr << "DB failed: " << e.what() << std::endl;
            return grpc::Status(grpc::StatusCode::INTERNAL, "Database error!");
        }

        response->set_job_id(job_id);
        std::cout << "[Master] Assigned" << job_id << " to worker '" << assigned_worker_id << "'" << std::endl;

        return Status::OK;
    }

    Status CoordinatorServiceImpl::Heartbeat(grpc::ServerContext* context, const mini_borg::HeartbeatRequest* request,
                                             mini_borg::HeartbeatResponse* response) {
        std::lock_guard<std::mutex> lock(map_mutex_);

        // liveliness update logic
        auto it = worker_map_.find(request->worker_id());
        auto worker_id = request->worker_id();
        if (it != worker_map_.end()) {
            it->second.last_heartbeat = std::chrono::steady_clock::now();
            std::cout << worker_id << " is alive." << std::endl;
        } else {
            const auto& resource = request->available_resources();

            // Re-adding worker after master crashes
            int actual_cpu = resource.cpu_cores();
            uint64_t actual_ram = resource.memory_mb();

            // Check if worker resources are currently being held
            auto res_it = reserved_resources_.find(worker_id);
            if (res_it != reserved_resources_.end()) {
                // The actual resource = amount of resource worker believes it has - amount server knows is reserved for
                // job processing
                actual_cpu -= res_it->second.cpu_cores();
                actual_ram -= res_it->second.memory_mb();

                std::cout << "[Recovery] Recovered resources for" << worker_id << std::endl;

                reserved_resources_.erase(res_it);
            }

            // make new worker node (or readd zombie node)
            worker_map_[request->worker_id()] = {request->worker_id(), actual_cpu, static_cast<int>(actual_ram),
                                                 std::chrono::steady_clock::now()};
            std::cout << "Worker " << worker_id << " added/recovered!" << std::endl;
        }

        // Logic to assign QUEUED jobs to correct worker (assigned jobs)
        auto queue_it = pending_jobs_map_.find(worker_id);

        if (queue_it != pending_jobs_map_.end() && !queue_it->second.empty()) {
            std::cout << "[Master] Dispatching " << queue_it->second.size() << " jobs to worker " << worker_id
                      << std::endl;

            for (const auto& job : queue_it->second) {
                *response->add_jobs_to_start() = job;  // repeated field in protobuf
                // Mark as RUNNING (2) so it doesn't get re-dispatched if the Master crashes
                store_->UpdateJobStatus(job.id(), 2);
            }

            queue_it->second.clear();
        }

        // Logic to assign QUEUED jobs to any worker (unassigned jobs)
        auto& worker = worker_map_[worker_id];

        while (!unassigned_queue_.empty()) {

            mini_borg::Job front_job = unassigned_queue_.front();

            int req_cpu = front_job.resource_reqs().cpu_cores();
            int req_ram = front_job.resource_reqs().memory_mb();

            if (worker.cpu_cores >= req_cpu && worker.ram_mb >= req_ram) {
                worker.cpu_cores -= req_cpu;
                worker.ram_mb -= req_ram;

                front_job.set_worker_id(worker_id);
                *response->add_jobs_to_start() = front_job;

                store_->UpdateJobStatus(front_job.id(), 2, worker_id);

                unassigned_queue_.pop_front();
            } else {
                break;
            }

        }

        return Status::OK;
    }
};  // namespace mini_borg
