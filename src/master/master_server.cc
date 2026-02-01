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

    std::string db_url = "host=" + db_host + " port=5432" + " dbname=miniborg" + " user=" + db_user +
                         " password=" + db_password + " sslmode=disable";

    std::cout << "[Config] Connecting to DB at " << db_host << " as user " << db_user << std::endl;

    auto store = std::make_unique<mini_borg::PostgresJobStore>(db_url);
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