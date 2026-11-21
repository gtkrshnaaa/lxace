# Multi-stage Docker build for LXACE
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN rm -rf build && mkdir -p build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make -j4 lxace lxace_tests

# Binary export stage for host installation via BuildKit
FROM scratch AS artifacts
COPY --from=builder /app/build/lxace /lxace
COPY --from=builder /app/build/lxace_tests /lxace_tests

# Production runtime stage
FROM ubuntu:24.04 AS runner

# Match the invoking user's uid/gid so a bind-mounted workspace stays writable. A fixed
# uid 10001 was shadowed by the bind mount, which made every fs_write fail at runtime.
#
# The base image already owns uid/gid 1000 (the stock `ubuntu` account), so creating a
# named account at that id fails with groupadd exit 4. Running as a numeric uid instead
# needs no passwd entry and never collides, while staying unprivileged.
ARG APP_UID=1000
ARG APP_GID=1000

WORKDIR /workspace
COPY --from=builder /app/build/lxace /usr/local/bin/lxace
COPY --from=builder /app/build/lxace_tests /usr/local/bin/lxace_tests

RUN chown -R "${APP_UID}:${APP_GID}" /workspace

ENV HOME=/workspace
USER ${APP_UID}:${APP_GID}

EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/lxace"]
