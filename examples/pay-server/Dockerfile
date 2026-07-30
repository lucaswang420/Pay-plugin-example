# Multi-stage build for the drogon-pay example host (PayServer).
# Build context is the repository root; the image needs libs/drogon-pay and
# examples/pay-server, so build with:
#   docker build -f examples/pay-server/Dockerfile .
# Stage 1: Build stage
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    libpq-dev \
    libssl-dev \
    perl \
    wget \
    git \
    unzip \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install Conan
RUN pip3 install conan

# Set working directory
WORKDIR /build

# Resolve dependencies first so the (expensive) Conan layer is cached until
# conanfile.py / conan.lock actually change.
COPY conanfile.py conan.lock ./
RUN conan profile detect --force && \
    conan install . --output-folder=build/linux-release --lockfile=conan.lock \
        -s build_type=Release -s compiler.cppstd=17 --build=missing

# Copy source code (repository root context)
COPY . .

# Build application (library + example host only; tests are not shipped)
RUN cmake --preset linux-release -DDROGON_PAY_BUILD_TESTS=OFF && \
    cmake --build --preset linux-release -j$(nproc)

# Stage 2: Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libpq5 \
    openssl \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -u 1000 payuser && \
    mkdir -p /app/config /app/certs /app/logs && \
    chown -R payuser:payuser /app

# Set working directory
WORKDIR /app

# Copy built binaries from builder stage. Only the server binary belongs in the
# production image; the test binary must not ship to prod.
COPY --from=builder /build/build/linux-release/examples/pay-server/PayServer /app/PayServer

# Copy configuration. Sensitive values in config.json use __env_var:NAME__
# placeholders resolved at runtime from the environment, so shipping the file is
# safe; the previous COPY referenced a non-existent config.example.json which
# broke the image build.
COPY examples/pay-server/config.json /app/config.json

# Change ownership
RUN chown payuser:payuser /app/PayServer /app/config.json

# Switch to non-root user
USER payuser

# Expose port
EXPOSE 5566

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:5566/healthz || exit 1

# Run application
CMD ["./PayServer"]
