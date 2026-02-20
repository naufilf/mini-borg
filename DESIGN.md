# Design Document: Mini-Borg Scheduler

## 1. Context & Scope
The goal of this project is to build a simplified distributed cluster manager capable of scheduling tasks onto worker nodes based on real-time resource availability. The system is designed to handle node failures gracefully, prevent resource over-subscription during network partitions, and persist job states for crash recovery.

## 2. Architecture Design

### 2.1 Communication Model (gRPC)
Chose gRPC over REST for its strict typing (Protobuf) and low-latency multiplexing.
- **Heartbeat Pattern:** Workers push state to the Master ("I am alive, here is my capacity") rather than the Master polling Workers. This heavily reduces Master complexity and network bottlenecking.

### 2.2 Concurrency Model
- **Master:** Uses coarse-grained locking (`std::mutex`) on the `worker_map` to ensure thread safety when concurrently processing heartbeats, state reconciliation, and job submissions.
- **Worker:** Uses `std::thread` with `.detach()` to execute jobs asynchronously. This prevents long-running tasks from blocking the main heartbeat loop, ensuring the Master does not falsely flag busy nodes as dead.

### 2.3 Fault Tolerance (The Reaper)
A background thread (`CheckDeadWorkers`) runs on the Master every 5 seconds.
- **Logic:** `if (Now - LastHeartbeat > 10s) -> Remove Worker`.
- **Trade-off:** Accepted a potential 10-15s delay in detecting hardware failures in exchange for simpler logic and reduced baseline network chatter.

### 2.4 Crash Recovery & State Reconciliation
The Master is completely stateless in RAM and treats PostgreSQL as the source of truth.
- **Boot Sequence:** On startup, the Master queries the DB for jobs in the `QUEUED` state and calculates a "Resource Debt" for each assigned worker.
- **Handshake Synchronization:** When a worker reconnects, the Master subtracts the calculated debt from the worker's reported free capacity. This "Add Debt / Subtract Capacity" pattern prevents the Master from over-subscribing a worker that is already committed to queued jobs.

## 3. Key Decisions & Trade-offs

| Decision | Alternative | Reason for Choice |
| :--- | :--- | :--- |
| **Push-Based Heartbeats** | Master Pings Workers | Push is more scalable; Master doesn't need to track Worker IPs beforehand. |
| **In-Memory State + DB** | DB Only | Storing worker state (RAM/CPU) in memory is significantly faster for scheduling decisions. DB serves strictly as a persistence layer for job history. |
| **Idempotent RPCs** | Assume Delivery | Worker retries `FinishJob` until acknowledged. Master checks DB state before reclaiming resources to prevent double-counting from network retries. |

## 4. Life of a Job
1. **Submission:** Client sends `SubmitJobRequest`.
2. **Scheduling:** Master locks the map, finds a worker with `AvailCPU > ReqCPU`.
3. **Allocation:** Master deducts resources immediately in memory and saves job to DB as `QUEUED`.
4. **Dispatch:** Next heartbeat from that Worker carries the job payload. Master immediately updates DB to `RUNNING` to prevent duplicate dispatch on crash.
5. **Execution:** Worker spawns a detached thread, simulates work, then loops `FinishJob` RPC until the Master returns an OK status.
6. **Reclamation:** Master updates DB to `COMPLETED` and returns resources to the cluster pool.