# Stage 1: Download and verify
FROM eclipse-temurin:17-jre-alpine AS downloader
WORKDIR /download
RUN apk add --no-cache curl
ARG LESHAN_VERSION=2.0.0-M14
RUN curl -fSL -o leshan-demo-server.jar \
    "https://ci.eclipse.org/leshan/job/leshan-ci/job/master/lastSuccessfulBuild/artifact/leshan-demo-server.jar"

# Stage 2: Runtime
FROM eclipse-temurin:17-jre-alpine AS runtime
LABEL maintainer="Miguel Fornell S."
LABEL description="Eclipse Leshan LWM2M Server for integration testing"

# Create non-root user
RUN addgroup -g 1000 leshan && \
    adduser -u 1000 -G leshan -D leshan

WORKDIR /app

# Copy jar from downloader stage
COPY --from=downloader /download/leshan-demo-server.jar .

# Create directories
RUN mkdir -p /var/log/leshan /data && \
    chown -R leshan:leshan /app /var/log/leshan /data

# Install curl for healthcheck
RUN apk add --no-cache curl

USER leshan

EXPOSE 8080 5683/udp 5684/udp

ENTRYPOINT ["java", "-jar", "leshan-demo-server.jar"]
CMD ["-wp", "8080", "-lp", "5683", "-slp", "5684"]
