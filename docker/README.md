# Development Environment

This folder contains the Docker and Compose configurations to ensure a reproducible build environment.

## Quick Start
1. Start the environment: `docker compose up -d`
2. Access the Docker environment: `docker exec -it zephyr-zenoh-dev bash'

## Mount Points
- `zephyr_zenoh_transport/` is shared across both containers to ensure serialization logic is always in sync.
- Network mode is set to `host` to allow Zenoh discovery across the containers and external devices.
