#include "src/master/postgres_job_store.h"

namespace mini_borg {
    void PostgresJobStore::SaveJobToDb(const std::string& id, const std::string& name, const Resource& res,
                                       const std::string& worker_id) {
        try {
            if (!db_conn_.is_open()) return;

            std::lock_guard<std::mutex> lock(db_mutex_);

            pqxx::work txn(db_conn_);  // transaction start
            std::string sql =
                "INSERT INTO jobs (id, name, cpu_req, ram_req, status, worker_id) "
                "VALUES ($1, $2, $3, $4, $5, $6)";

            txn.exec_params(sql, id, name, res.cpu_cores(), res.memory_mb(), 1, worker_id);
            txn.commit();
            std::cout << "[DB] Job " << id << " persisted." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[DB Error] " << e.what() << std::endl;
        }
    }

    void PostgresJobStore::UpdateJobStatus(const std::string& job_id, int status_enum) {
        try {
            if (!db_conn_.is_open()) return;

            std::lock_guard<std::mutex> lock(db_mutex_);

            pqxx::work txn(db_conn_);  // transaction start
            std::string sql = "UPDATE jobs SET status = $1 WHERE id = $2";

            txn.exec_params(sql, status_enum, job_id);
            txn.commit();

            // cast to string for logging
            std::string status_str = mini_borg::JobStatus_Name(static_cast<mini_borg::JobStatus>(status_enum));
            std::cout << "[DB] Job " << job_id << " updated " << "to " << status_str << "." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[DB Error] " << e.what() << std::endl;
        }
    }

    int PostgresJobStore::GetJobStatusFromDB(const std::string& job_id) {
        try {
            std::lock_guard<std::mutex> lock(db_mutex_);
            if (!db_conn_.is_open()) return -1;

            // Use nontransaction for read-only queries (faster/safer)
            pqxx::nontransaction N(db_conn_);

            std::string sql = "SELECT status FROM jobs WHERE id = $1";
            auto result = N.exec_params(sql, job_id);

            if (result.empty()) {
                return -1;  // Job not found
            }

            // Return the integer value from the first column of the first row
            return result[0][0].as<int>();
        } catch (const std::exception& e) {
            std::cerr << "[DB Error] GetJobStatusFromDb failed: " << e.what() << std::endl;
            return -1;
        }
    }
}  // namespace mini_borg