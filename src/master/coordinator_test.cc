#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/master/coordinator_service.h"
#include "src/master/mock_job_store.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

namespace mini_borg {
    class CoordinatorServiceTest : public ::testing::Test {
    protected:
        // runs before every TEST_F
        void SetUp() override {
            auto mock = std::make_unique<MockJobStore>();

            // the mock will lose its ownership to the service in tghe next line so we store raw pointer here to keep
            // track of test expectations
            store_mock_ = mock.get();

            service_ = std::make_unique<CoordinatorServiceImpl>(std::move(mock));
        }

        MockJobStore* store_mock_;
        std::unique_ptr<CoordinatorServiceImpl> service_;
        grpc::ServerContext context_;
    };

    TEST_F(CoordinatorServiceTest, SubmitJobToDb_PersistsToDatabase_WhenResourcesAvailable) {
        HeartbeatRequest hb_request;
        hb_request.set_worker_id("worker-01");
        hb_request.mutable_available_resources()->set_cpu_cores(8);
        hb_request.mutable_available_resources()->set_memory_mb(4000);

        HeartbeatResponse hb_response;

        service_->Heartbeat(&context_, &hb_request, &hb_response);

        SubmitJobRequest request;
        request.set_name("test-job");
        request.mutable_resource_reqs()->set_cpu_cores(4);
        request.mutable_resource_reqs()->set_memory_mb(2000);

        SubmitJobResponse response;

        EXPECT_CALL(*store_mock_, SaveJobToDb(_, Eq("test-job"), _, Eq("worker-01"))).Times(1);
        grpc::Status status = service_->SubmitJob(&context_, &request, &response);
        EXPECT_TRUE(status.ok());
    }

    TEST_F(CoordinatorServiceTest, SubmitJobToDb_ReturnsError_WhenDbFails) {
        HeartbeatRequest hb_request;
        hb_request.set_worker_id("worker-01");
        hb_request.mutable_available_resources()->set_cpu_cores(8);
        hb_request.mutable_available_resources()->set_memory_mb(4000);

        HeartbeatResponse hb_response;

        service_->Heartbeat(&context_, &hb_request, &hb_response);

        SubmitJobRequest request;
        request.set_name("test-job");
        request.mutable_resource_reqs()->set_cpu_cores(2);
        request.mutable_resource_reqs()->set_memory_mb(2000);

        SubmitJobResponse response;

        EXPECT_CALL(*store_mock_, SaveJobToDb(_, _, _, _))
            .WillOnce(::testing::Throw(std::runtime_error("DB Connection Lost")));

        grpc::Status status = service_->SubmitJob(&context_, &request, &response);

        EXPECT_FALSE(status.ok());
        EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    }
}  // namespace mini_borg