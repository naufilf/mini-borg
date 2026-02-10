# 1. Start with standard Ubuntu (supports both Mac M1/M2 and Intel)
FROM ubuntu:22.04

# 2. Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# 3. Install tools
# Added 'openjdk-11-jdk': Required by many Bazel rules (like Protobuf/gRPC) to run tools.
RUN apt-get update && apt-get install -y \
    build-essential \
    libpq-dev \
    libpqxx-dev \
    curl \
    git \
    python3 \
    openjdk-11-jdk \
    && rm -rf /var/lib/apt/lists/*

# 4. Install Bazelisk
RUN ARCH=$(dpkg --print-architecture) && \
    curl -fsSL "https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-${ARCH}" -o /usr/local/bin/bazel && \
    chmod +x /usr/local/bin/bazel

# 5. Set up workspace
WORKDIR /app


COPY .bazelversion .bazelversion
COPY MODULE.bazel MODULE.bazel

# 6. Copy the rest of the source code
COPY . .

RUN bazel build //src/master:master_server //src/worker:worker_client_bin