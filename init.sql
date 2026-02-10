-- init.sql
\c miniborg;
CREATE TABLE IF NOT EXISTS jobs (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100),
    cpu_req INT,          
    ram_req INT,         
    status INT,
    worker_id VARCHAR(50)
);