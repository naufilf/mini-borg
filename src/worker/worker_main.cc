#include <grpcpp/grpcpp.h>

#include <chrono>  // For seconds
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>  // For sleep
#include <vector>

#include "src/worker/worker_client.h"

// GLOBAL STATE
std::mutex g_resource_mutex;
int g_current_cpu = 0;  // Will be set in main
int g_current_ram = 0;  // Will be set in main

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

void RunTask(mini_borg::WorkerClient* client, std::string worker_id, mini_borg::Job job) {
    std::cout << "[Worker] Starting job " << job.id() << " on " << worker_id << std::endl;
    // simulating work
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // RELEASE RESOURCES
    {
        std::lock_guard<std::mutex> lock(g_resource_mutex);
        g_current_cpu += job.resource_reqs().cpu_cores();
        g_current_ram += job.resource_reqs().memory_mb();
        std::cout << "[Worker] Job done. Freed resources. Current CPU: " << g_current_cpu
                  << " Current Memory: " << g_current_ram << std::endl;
    }

    mini_borg::Resource released_resources;
    released_resources.set_cpu_cores(job.resource_reqs().cpu_cores());
    released_resources.set_memory_mb(job.resource_reqs().memory_mb());

    client->NotifyJobFinished(job.id(), worker_id, true, released_resources);
}

int main(int argc, char** argv) {
    std::string target_str = GetFlag(argc, argv, "--coordinator_addr", "localhost:50051");
    // parse args for worker creation
    std::string worker_id = GetFlag(argc, argv, "--name", "worker-default");
    int total_cpu = std::atoi(GetFlag(argc, argv, "--cpu", "8").c_str());
    int total_ram = std::atoi(GetFlag(argc, argv, "--ram", "4096").c_str());

    // Initialize Global State
    g_current_cpu = total_cpu;
    g_current_ram = total_ram;

    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    mini_borg::WorkerClient client(channel);

    std::cout << "[Worker] Startup: Connecting to " << target_str << std::endl;

    while (true) {
        std::vector<mini_borg::Job> jobs;
        int report_cpu, report_ram;
        {
            std::lock_guard<std::mutex> lock(g_resource_mutex);
            report_cpu = g_current_cpu;
            report_ram = g_current_ram;
        }

        bool success = client.SendHeartbeat(worker_id, report_cpu, report_ram, &jobs);

        if (success) {
            std::cout << "[SUCCESS] Message sent" << std::endl;
            if (!jobs.empty()) {
                for (const auto& job : jobs) {
                    // DEDUCT RESOURCES IMMEDIATELY
                    {
                        std::lock_guard<std::mutex> lock(g_resource_mutex);
                        g_current_cpu -= job.resource_reqs().cpu_cores();
                        g_current_ram -= job.resource_reqs().memory_mb();
                        std::cout << "[Worker] Resources after deduction for job are CPU: " << g_current_cpu
                                  << " Memory: " << g_current_ram << std::endl;
                    }
                    std::thread job_thread(RunTask, &client, worker_id, job);
                    job_thread.detach();
                }
            }
        } else {
            std::cout << "[FAIL] Message failed to send" << std::endl;
        }
        // prevents server from getting flooded
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return 0;
}