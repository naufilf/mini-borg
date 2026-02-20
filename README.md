# Mini-Borg Scheduler

A lightweight, containerized distributed cluster scheduler inspired by Kubernetes and Google Borg. Mini-Borg manages resource allocation, state reconciliation, and fault tolerance across a fleet of worker nodes using a strongly-typed C++ control plane, a Java API gateway, and gRPC communication.

    Client [HTTP POST] -> Gateway [Java Spring Boot] -> Master [C++ Coordinator] -> Workers [C++ Nodes]
                                                                  |
                                                                  v
                                                           Database [PostgreSQL]

## Tech Stack
* **Control Plane & Workers:** C++17
* **API Gateway:** Java Spring Boot
* **RPC Framework:** gRPC & Protobuf
* **Build Systems:** Bazel (C++) / Gradle (Java)
* **Infrastructure:** Docker Compose, PostgreSQL

## Quick Start

The infrastructure is entirely containerized and requires no local dependencies other than Docker.

**1. Start the Cluster**
```bash
    docker-compose up --build -d
```

**2. Scale Worker Nodes**
Dynamically provision additional worker nodes to simulate a larger cluster environment:
```bash
    docker-compose up -d --scale worker=3
```

**3. Submit a Job**
Queue a task via the API Gateway (Port 8080):
```bash
    curl -X POST http://localhost:8080/submit \
      -H "Content-Type: application/json" \
      -d '{
        "name": "render-video-task",
        "cpu_cores": 2,
        "memory_mb": 1024
      }'
```
**4. Monitor the Control Plane**
Observe scheduling, resource allocation, and state reconciliation in real-time:

```bash
    docker-compose logs -f coordinator
```

## Roadmap
* **Fault Tolerance:** Implement a retry queue for jobs that fail mid-execution or trigger worker reaps.
* **Observability:** Develop a React frontend to visualize real-time cluster resource utilization and node health.
* **High Availability:** Integrate etcd or ZooKeeper for Master leader election.