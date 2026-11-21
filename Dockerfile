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
ARG APP_UID=1000
ARG APP_GID=1000

RUN groupadd --gid "${APP_GID}" appuser 2>/dev/null \
    && useradd --uid "${APP_UID}" --gid "${APP_GID}" -m -s /bin/bash appuser

WORKDIR /workspace
COPY --from=builder /app/build/lxace /usr/local/bin/lxace
COPY --from=builder /app/build/lxace_tests /usr/local/bin/lxace_tests

RUN chown -R "${APP_UID}:${APP_GID}" /workspace
USER appuser

EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/lxace"]
