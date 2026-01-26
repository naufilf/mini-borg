#include <iostream>
#include <string>
#include <vector>
#include <thread> // For sleep
#include <chrono> // For seconds

#include <grpcpp/grpcpp.h>
#include "src/worker/worker_client.h"

void RunTask(mini_borg::WorkerClient *client, std::string worker_id, mini_borg::Job job)
{
    std::cout << "[Worker] Starting job " << job.id() << " on " << worker_id << std::endl;
    // simulating work
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "[Worker] Finished job " << job.id() << "." << std::endl;
    client->NotifyJobFinished(job.id(), worker_id, true, job.resource_reqs());
}

int main(int argc, char **argv)
{
    std::string target_str = "localhost:50051";

    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    mini_borg::WorkerClient client(channel);

    std::string my_id = "worker-01";
    std::cout << "[Worker] Startup: Connecting to " << target_str << std::endl;

    while (true)
    {
        std::vector<mini_borg::Job> jobs;
        bool success = client.SendHeartbeat(my_id, 16, 16000, &jobs);
        if (success)
        {
            std::cout << "[SUCCESS] Message sent" << std::endl;
            if (!jobs.empty())
            {
                for (const auto &job : jobs)
                {
                    // thread simulating work will run on its own (not with main)
                    // avoids server's reaper thread marking worker as dead
                    std::thread worker_thread(RunTask, &client, my_id, job);
                    worker_thread.detach();
                }
            }
        }
        else
        {
            std::cout << "[FAIL] Message failed to send" << std::endl;
        }
        // prevents server from getting flooded
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    return 0;
}