# Design Document: Mini-Borg Scheduler

## 1. Context & Scope

The goal was to build a simplified distributed cluster manager capable of scheduling tasks onto worker nodes based on resource availability. The system must handle node failures gracefully and persist job states.

## 2. Architecture Design

### 2.1 Communication Model (gRPC)

We chose gRPC over REST for its strict typing (Protobuf) and performance.

- **Service Definition:** `scheduler.proto` defines the `Coordinator` service.
- **Heartbeat Pattern:** Workers push state to Master ("I am alive") rather than Master pulling from Workers. This reduces Master complexity.

### 2.2 Concurrency Model

- **Master:** Uses coarse-grained locking (`std::mutex`) on the `worker_map` to ensure thread safety when processing heartbeats and job submissions simultaneously.
- **Worker:** Uses `std::thread` with `.detach()` to execute jobs. This prevents long-running tasks from blocking the main heartbeat loop, which would otherwise cause the Master to mark the node as dead.

### 2.3 Fault Tolerance (The Reaper)

A background thread (`CheckDeadWorkers`) runs on the Master every 5 seconds.

- **Logic:** `if (Now - LastHeartbeat > 10s) -> Delete Worker`.
- **Trade-off:** We accepted a potential 10-15s delay in detecting failures in exchange for simpler logic and reduced network chatter.

### 2.4 Data Persistence

- **Database:** PostgreSQL.
- **Consistency:** The Master writes to DB _before_ acknowledging job submission to the client to ensure durability.

## 3. Key Decisions & Trade-offs

| Decision                  | Alternative          | Reason for Choice                                                                                             |
| :------------------------ | :------------------- | :------------------------------------------------------------------------------------------------------------ |
| **Push-Based Heartbeats** | Master Pings Workers | Push is more scalable; Master doesn't need to know IP addresses of Workers beforehand.                        |
| **In-Memory State + DB**  | DB Only              | Storing worker state (RAM/CPU) in memory is faster for scheduling decisions. DB is used only for job history. |
| **Mutex Locking**         | Read/Write Locks     | Standard Mutex is simpler to implement for this scale. Contention is low.                                     |

## 4. Life of a Job

1.  **Submission:** Client sends `SubmitJobRequest`.
2.  **Scheduling:** Master locks map, finds a worker with `AvailCPU > ReqCPU`.
3.  **Allocation:** Master deducts resources immediately in memory.
4.  **Queuing:** Job is added to `pending_jobs` queue for that specific worker.
5.  **Dispatch:** Next heartbeat from that Worker carries the job payload.
6.  **Execution:** Worker spawns thread, sleeps (simulating work), then calls `FinishJob`.
7.  **Reclamation:** Master updates DB status and returns resources to the pool.

```

```
