#ifndef MINI_BORG_MASTER_JOB_STORE_H_
#define MINI_BORG_MASTER_JOB_STORE_H_
#include <grpcpp/grpcpp.h>

#include <string>

#include "src/proto/scheduler.grpc.pb.h"  //  for Resource

namespace mini_borg {
    class JobStore {
    public:
        virtual ~JobStore() = default;
        virtual void SaveJobToDb(const std::string& id, const std::string& name, const mini_borg::Resource& res,
                                 const std::string& worker_id) = 0;
        virtual int GetJobStatusFromDB(const std::string& job_id) = 0;
        virtual std::vector<mini_borg::Job> RequeueOrphanedJobs(const std::string& dead_worker_id) = 0;
        virtual void UpdateJobStatus(const std::string& job_id, int status_enum, const std::string& worker_id = "") = 0;
        virtual std::vector<mini_borg::Job> GetJobsOfStatusFromDB(int status_enum) = 0;
    };
}  // namespace mini_borg

#endif