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

# --- OPTIMIZATION START ---
# Copy dependency definitions first. 
# This allows Docker to cache the "fetch" step if your dependencies haven't changed.
COPY .bazelversion .bazelversion
COPY MODULE.bazel MODULE.bazel
# COPY MODULE.bazel.lock MODULE.bazel.lock  <-- Uncomment if you have a lockfile
# --- OPTIMIZATION END ---

# 6. Copy the rest of the source code
COPY . .

# 7. Default command
CMD ["/bin/bash"]