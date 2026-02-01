#ifndef MINI_BORG_MASTER_POSTGRES_JOB_STORE_H_
#define MINI_BORG_MASTER_POSTGRES_JOB_STORE_H_

#include <iostream>
#include <mutex>
#include <pqxx/pqxx>

#include "src/master/job_store.h"

namespace mini_borg {

    class PostgresJobStore : public JobStore {
    public:
        explicit PostgresJobStore(const std::string& conn_str) : db_conn_(conn_str) {
            if (db_conn_.is_open()) {
                std::cout << "[DB] Connected to " << db_conn_.dbname() << std::endl;
            } else {
                std::cerr << "[DB] Failed to open database!" << std::endl;
            }
        }

        void SaveJobToDb(const std::string& id, const std::string& name, const Resource& res,
                         const std::string& worker_id) override;
        int GetJobStatusFromDB(const std::string& job_id) override;
        void UpdateJobStatus(const std::string& job_id, int status_enum) override;

    private:
        pqxx::connection db_conn_;
        std::mutex db_mutex_;
    };

}  // namespace mini_borg

#endif