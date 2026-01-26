#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "src/master/coordinator_service.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer()
{
    std::string server_address("0.0.0.0:50051");
    std::string db_url = "host=host.docker.internal port=5432 dbname=miniborg user=naufilfaruqi sslmode=disable";

    mini_borg::CoordinatorServiceImpl service(db_url);
    grpc::ServerBuilder builder;
    // TODO: add authentication mechanism
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // Register service as the instance through which we interact with clients
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char **argv)
{
    RunServer();
    return 0;
}