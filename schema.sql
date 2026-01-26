CREATE TABLE IF NOT EXISTS jobs (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    cpu_req INT,
    ram_req INT,
    status INT,
    worker_id TEXT
);