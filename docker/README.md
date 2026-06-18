# Development Environment

This folder contains the Docker and Compose configurations to ensure a reproducible build environment.

## Quick Start
1. Start the environment: `docker compose up -d`
2. Access the Docker environment: `docker compose exec dev-env bash'

## Mount Points
- `zenbedded_transport/` is shared across both containers to ensure serialization logic is always in sync.
- `zenbedded_rcl/` and `demos/` are mounted into the Zephyr workspace.
- Network mode is set to `host` to allow Zenoh discovery across the containers and external devices.
