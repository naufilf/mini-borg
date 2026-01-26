#include "src/master/coordinator_service.h"
#include <pqxx/pqxx>
#include <iostream>

namespace mini_borg
{

    using grpc::ServerContext;
    using grpc::Status;

    // initialize DB connection immediately
    CoordinatorServiceImpl::CoordinatorServiceImpl(const std::string &db_conn_str) : db_conn_(db_conn_str)
    {
        running_ = true;
        if (db_conn_.is_open())
        {
            std::cout << "[Master] Connected to Database: " << db_conn_.dbname() << std::endl;
        }
        else
        {
            std::cerr << "[Master] Failed to open database!" << std::endl;
        }
        // this thread now constantly runs check dead workers
        reaper_thread_ = std::thread(&CoordinatorServiceImpl::CheckDeadWorkers, this);
    }

    CoordinatorServiceImpl::~CoordinatorServiceImpl()
    {
        running_ = false;
        if (reaper_thread_.joinable())
        {
            reaper_thread_.join();
        }
    }

    void CoordinatorServiceImpl::CheckDeadWorkers()
    {
        try
        {
            while (running_)
            {
                std::this_thread::sleep_for(std::chrono::seconds(5));

                std::lock_guard<std::mutex> lock(map_mutex_);
                std::cout << "[Reaper] Scanning for dead workers..." << std::endl;

                auto now = std::chrono::steady_clock::now();

                for (auto it = worker_map_.begin(); it != worker_map_.end();)
                {
                    auto last_seen = it->second.last_heartbeat;
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_seen).count();

                    std::cout << "Worker: " << it->first
                              << " | CPU: " << it->second.cpu_cores
                              << " | RAM: " << it->second.ram_mb
                              << " | Silence: " << duration << "s" << std::endl;

                    if (duration > 10)
                    { // if the worker silent for this long, its dead
                        std::cout << "[Master] Worker " << it->first << " is DEAD. Removing now..." << std::endl;
                        it = worker_map_.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Master] Reaper thread error: " << e.what() << std::endl;
        }
    }

    void CoordinatorServiceImpl::SaveJobToDb(const std::string &id, const std::string &name, const Resource &res, const std::string &worker_id)
    {
        try
        {
            if (!db_conn_.is_open())
                return;

            std::lock_guard<std::mutex> lock(db_mutex_);

            pqxx::work txn(db_conn_); // transaction start
            std::string sql = "INSERT INTO jobs (id, name, cpu_req, ram_req, status, worker_id) "
                              "VALUES ($1, $2, $3, $4, $5, $6)";

            txn.exec_params(sql, id, name, res.cpu_cores(), res.memory_mb(), 1, worker_id); // TODO: change hardcoded status from 1
            txn.commit();
            std::cout << "[DB] Job " << id << " persisted." << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[DB Error] " << e.what() << std::endl;
        }
    }

    void CoordinatorServiceImpl::UpdateJobStatus(const std::string &job_id, int status_enum)
    {
        try
        {
            if (!db_conn_.is_open())
                return;

            std::lock_guard<std::mutex> lock(db_mutex_);

            pqxx::work txn(db_conn_); // transaction start
            std::string sql = "UPDATE jobs SET status = $1 WHERE id = $2";

            txn.exec_params(sql, status_enum, job_id);
            txn.commit();

            // cast to string for logging
            std::string status_str = mini_borg::JobStatus_Name(static_cast<mini_borg::JobStatus>(status_enum));
            std::cout << "[DB] Job " << job_id << " updated " << "to " << status_str << "." << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[DB Error] " << e.what() << std::endl;
        }
    }

    int CoordinatorServiceImpl::GetJobStatusFromDB(const std::string &job_id)
    {
        try
        {
            std::lock_guard<std::mutex> lock(db_mutex_);
            if (!db_conn_.is_open())
                return -1;

            // Use nontransaction for read-only queries (faster/safer)
            pqxx::nontransaction N(db_conn_);

            std::string sql = "SELECT status FROM jobs WHERE id = $1";
            auto result = N.exec_params(sql, job_id);

            if (result.empty())
            {
                return -1; // Job not found
            }

            // Return the integer value from the first column of the first row
            return result[0][0].as<int>();
        }
        catch (const std::exception &e)
        {
            std::cerr << "[DB Error] GetJobStatusFromDb failed: " << e.what() << std::endl;
            return -1;
        }
    }

    Status CoordinatorServiceImpl::FinishJob(grpc::ServerContext *context, const mini_borg::FinishJobRequest *request, mini_borg::FinishJobResponse *response)
    {
        // ONLY save to db if job hasnt been cancelled, this is quick solution
        int database_status = GetJobStatusFromDB(request->job_id());
        if (database_status != mini_borg::JOB_STATUS_CANCELLED)
        {
            // check if job was a success
            int status = request->success() ? mini_borg::JOB_STATUS_COMPLETED : mini_borg::JOB_STATUS_FAILED;
            UpdateJobStatus(request->job_id(), status);
        }
        else
        {
            std::cout << "[Master] Ignoring FinishJob for cancelled job " << request->job_id() << std::endl;
        }

        auto it = worker_map_.find(request->worker_id());
        // reallocate resources
        // SAFETY CHECK: Does the worker still exist in our map?
        if (it != worker_map_.end())
        {
            it->second.cpu_cores += request->released_resources().cpu_cores();
            it->second.ram_mb += request->released_resources().memory_mb();

            std::cout << "[Master] Reclaimed resources from " << request->worker_id() << std::endl;
        }
        else
        {
            // Edge Case: Worker was reaped while job was running.
            // still log in DB since worker might still exist in the case that worker is using 100% CPU on job and cant send heartbeat
            std::cout << "[Master] Warning: Worker " << request->worker_id() << " not found during FinishJob." << std::endl;
        }

        return Status::OK;
    }

    Status CoordinatorServiceImpl::SubmitJob(grpc::ServerContext *context, const mini_borg::SubmitJobRequest *request, mini_borg::SubmitJobResponse *response)
    {
        // assign job id
        std::string job_id;
        {
            std::lock_guard<std::mutex> lock(counter_mutex_);
            job_counter_++;
            job_id = "job-" + std::to_string(job_counter_);
        }

        std::string assigned_worker_id;

        // logic for assigning job to worker
        {
            std::lock_guard<std::mutex> map_lock(map_mutex_);

            if (worker_map_.empty())
            {
                return Status(grpc::StatusCode::UNAVAILABLE, "No workers detected");
            }

            for (auto &pair : worker_map_)
            {
                auto &worker = pair.second;
                if (worker.cpu_cores >= request->resource_reqs().cpu_cores() && worker.ram_mb >= request->resource_reqs().memory_mb())
                {
                    assigned_worker_id = worker.id;
                    // deduct resources
                    worker.cpu_cores -= request->resource_reqs().cpu_cores();
                    worker.ram_mb -= request->resource_reqs().memory_mb();
                    std::cout << "[Master] Now, " << assigned_worker_id << " has " << worker.cpu_cores << " cores and " << worker.ram_mb << " MB of memory available." << std::endl;

                    mini_borg::Job new_job;
                    new_job.set_id(job_id);
                    new_job.set_name(request->name());
                    new_job.set_worker_id(assigned_worker_id);
                    new_job.set_status(mini_borg::JOB_STATUS_QUEUED);
                    *new_job.mutable_resource_reqs() = request->resource_reqs();

                    pending_jobs_map_[assigned_worker_id].push_back(new_job);

                    break;
                }
            }
        }

        if (assigned_worker_id.empty())
        {
            return Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "No worker has enough resources");
        }

        // save job to db logic
        SaveJobToDb(job_id, request->name(), request->resource_reqs(), assigned_worker_id);

        response->set_job_id(job_id);
        std::cout << "[Master] Assigned" << job_id << " to worker '" << assigned_worker_id << "'" << std::endl;

        return Status::OK;
    }

    Status CoordinatorServiceImpl::Heartbeat(grpc::ServerContext *context, const mini_borg::HeartbeatRequest *request, mini_borg::HeartbeatResponse *response)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);

        // liveliness update logic
        auto it = worker_map_.find(request->worker_id());
        auto worker_id = request->worker_id();
        if (it != worker_map_.end())
        {
            it->second.last_heartbeat = std::chrono::steady_clock::now();
            std::cout << worker_id << " is alive." << std::endl;
        }
        else
        {
            const auto &resource = request->available_resources();
            // make new worker node
            worker_map_[request->worker_id()] = {
                request->worker_id(),
                resource.cpu_cores(),
                resource.memory_mb(),
                std::chrono::steady_clock::now()};
            std::cout << "Worker " << worker_id << " added!" << std::endl;
        }

        // queue checking logic
        auto queue_it = pending_jobs_map_.find(worker_id);

        if (queue_it != pending_jobs_map_.end() && !queue_it->second.empty())
        {
            std::cout << "[Master] Dispatching " << queue_it->second.size()
                      << " jobs to worker " << worker_id << std::endl;

            for (const auto &job : queue_it->second)
            {
                *response->add_jobs_to_start() = job; // repeated field in protobuf
            }

            queue_it->second.clear();
        }

        return Status::OK;
    }
}; // namespace mini_borg
