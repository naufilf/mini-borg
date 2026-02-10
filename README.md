Mini-Borg

A "toy" distributed cluster scheduler built to understand how systems like Kubernetes work under the hood. It features a C++ control plane, a Java API gateway, and a fleet of worker nodes, all communicating via gRPC.


This is a microservice architecture running entirely in Docker.


```
   User[User/Client] -- HTTP POST --> Gateway[Java API Gateway]
    Gateway -- gRPC --> Master[C++ Coordinator]
    Master -- gRPC --> Worker1[C++ Worker 1]
    Master -- gRPC --> Worker2[C++ Worker 2]
    Master -- SQL --> DB[(PostgreSQL)]
```

Here's the Stack:

    Core Logic: C++17 (Master & Workers)

    API Layer: Java Spring Boot

    RPC Framework: gRPC + Protobuf

    Build System: Bazel (for C++), Gradle (for Java)

    Infrastructure: Docker Compose

🚀 Quick Start

You don't need to install C++, Java, or Postgres locally. Everything runs in containers.
1. Spin up the Cluster
```Bash
docker-compose up --build
```

2. Scale the Workers

Want to simulate a larger cluster? You can scale the workers dynamically:
Bash
```Bash
docker-compose up -d --scale worker=3
```
You will see 3 distinct workers register in the Master logs.

3. Submit a Job

Use curl to hit the Java Gateway (exposed on port 8080):
```Bash
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/json" \
  -d '{
    "name": "render-video-task",
    "cpu_cores": 2,
    "memory_mb": 1024
  }'
```

Future Roadmap

    [ ] Implement a "Retry" queue for jobs that fail mid-execution.

    [ ] Add a React frontend to visualize cluster resource usage in real-time.

    [ ] Add etcd for Master leader election 