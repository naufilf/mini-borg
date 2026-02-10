#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

#include "src/master/coordinator_service.h"
#include "src/master/postgres_job_store.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    std::string server_address("0.0.0.0:50051");

    const char* env_user = std::getenv("DB_USER");
    std::string db_user = env_user ? env_user : "postgres";

    const char* env_pass = std::getenv("DB_PASSWORD");
    if (!env_pass) {
        std::cerr << "[FATAL] DB_PASSWORD environment variable is not set." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::string db_password(env_pass);

    const char* env_host = std::getenv("DB_HOST");
    std::string db_host = env_host ? env_host : "host.docker.internal";

    const char* env_db_name = std::getenv("DB_NAME");
    std::string db_name = env_db_name ? env_db_name : "miniborg";

    std::string db_url = "host=" + db_host + " port=5432" + " dbname=" + db_name + " user=" + db_user +
                         " password=" + db_password + " sslmode=disable";

    std::cout << "[Config] Connecting to DB at " << db_host << " as user " << db_user << std::endl;

    std::unique_ptr<mini_borg::PostgresJobStore> store;
    int max_retries = 10;

    for (int i = 0; i < max_retries; ++i) {
        try {
            store = std::make_unique<mini_borg::PostgresJobStore>(db_url);
            std::cout << "[Success] Connected to Database!" << std::endl;
            break;  // Connection successful, exit the loop
        } catch (const std::exception& e) {
            std::cerr << "[Warning] Failed to connect to DB (Attempt " << i + 1 << "/" << max_retries
                      << "): " << e.what() << std::endl;
            if (i == max_retries - 1) {
                std::cerr << "[FATAL] Could not connect to database after multiple attempts." << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::cout << "Retrying in 2 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    mini_borg::CoordinatorServiceImpl service(std::move(store));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // Register service as the instance through which we interact with clients
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}