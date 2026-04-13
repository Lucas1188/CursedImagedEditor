FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    curl \
    xz-utils \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# 1. Create a dedicated directory for Zig
WORKDIR /opt/zig

# 2. Download and extract (Note: using the exact URL you provided)
RUN curl -L https://ziglang.org/builds/zig-x86_64-linux-0.16.0-dev.3153+d6f43caad.tar.xz \
    | tar -xJ --strip-components=1 -C .

# 3. Create a symlink in the standard system PATH
RUN ln -s /opt/zig/zig /usr/local/bin/zig

# 4. Final verification inside the build (will fail build if zig is missing)
RUN zig version

WORKDIR /project
CMD ["make"]