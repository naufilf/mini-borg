#include "gmock/gmock.h"
#include "src/master/job_store.h"

namespace mini_borg {

    class MockJobStore : public JobStore {
    public:
        MOCK_METHOD(void, SaveJobToDb,
                    (const std::string& id, const std::string& name, const mini_borg::Resource& res,
                     const std::string& worker_id),
                    (override));
        MOCK_METHOD(int, GetJobStatusFromDB, (const std::string& job_id), (override));
        MOCK_METHOD(void, UpdateJobStatus, (const std::string& job_id, int status_enum), (override));
    };
}  // namespace mini_borg