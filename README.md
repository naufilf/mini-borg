# Mini-Borg: Distributed Job Scheduler

**Mini-Borg** is a lightweight distributed job scheduler built in C++ that mimics the core architecture of large-scale cluster management systems. It manages resource allocation, worker node health monitoring, and job execution across a distributed fleet.

## 🤖 Key Features

- **Master-Worker Architecture:** Centralized coordination with distributed execution.
- **Fault Tolerance:** "Reaper" thread detects and removes dead workers (Heartbeat mechanism).
- **Resource Scheduling:** Assigns jobs based on available CPU/RAM constraints.
- **Concurrency:** Multithreaded Worker nodes execute tasks without blocking heartbeats.
- **Persistence:** Job states and history are persisted to PostgreSQL.
- **gRPC & Protobuf:** High-performance, typed communication between nodes.

## 🛠 Tech Stack

- **Language:** C++17
- **Communication:** gRPC / Protocol Buffers
- **Build System:** Bazel
- **Containerization:** Docker
- **Database:** PostgreSQL (libpqxx)

## 🏗 Architecture

The system consists of three distinct components:

1.  **The Master (Coordinator):** Holds the source of truth. It maintains a memory map of all active workers and their resources, locks critical sections for thread safety, and persists job statuses to the DB.
2.  **The Worker (Node):** Registers itself with the Master, sends heartbeats every 3 seconds, and executes jobs in detached threads to simulate asynchronous work.
3.  **The Client (Submitter):** CLI tool that requests resources and submits jobs to the cluster.

## 📦 How to Run (Using Docker)

### Prerequisites

- Docker installed on your machine.

### 1. Build the Cluster Image

```bash
docker build -t miniborg .
```

### 2. Start the Master Server

```bash
docker run --rm -it \
  --name borg_cluster \
  -p 50051:50051 \
  -v "$(pwd)":/app \
  -v miniborg_cache:/root/.cache/bazel \
  miniborg \
  bazel run //src/master:master_server
```

### 3. Start a Worker Node

```bash
docker exec -it borg_cluster bazel run //src/worker:worker_node -- worker-01
```

### 4. Submit a Job

```bash
docker exec -it borg_cluster bazel run //src/client:submit_job
```

## 🛢 Database Schema

In order to set up the persistence layer, make sure your PostgreSQL instance runs the following:

```bash
CREATE TABLE IF NOT EXISTS jobs (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    cpu_req INT,
    ram_req INT,
    status INT,
    worker_id TEXT
);
```

## 🧩 Future Improvements

- Implement "Job Cancellation" propagation to workers.
- Add a Web UI to visualize the cluster state.
- Support dynamic resource updates from workers.
