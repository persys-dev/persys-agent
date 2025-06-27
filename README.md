# Persys Agent

Persys Agent is a C++-based system agent that manages Docker containers, Docker Compose services, Docker Swarm clusters, and cron jobs on a node. It communicates with a central server to report its status and receive commands.

## Features

- **Node Management**: Registration and health monitoring of nodes in the system
- **Docker Management**: Container lifecycle management (create, start, stop, remove)
- **Docker Compose**: Service management using Docker Compose
- **Docker Swarm**: Swarm cluster management and operations
- **Cron Job Management**: Schedule and manage cron jobs
- **System Resource Monitoring**: CPU and memory usage tracking
- **Heartbeat Mechanism**: Regular status updates to the central server
- **Secure Communication**: Request signature verification middleware

## Prerequisites

- C++17 or later
- libcurl
- jsoncpp
- Crow (C++ web framework)
- Docker
- Docker Compose
- Docker Swarm (optional)

## Environment Variables

- `CENTRAL_URL`: URL of the central server (required)
- `AGENT_PORT`: Port for the agent's HTTP server (default: 8080)

## API Endpoints

### Health Check
- `GET /api/v1/health`: Returns node health status and resource usage

### Docker Operations
- Container management endpoints for create, start, stop, and remove operations
- Container inspection and status checking

### Docker Compose
- Service deployment and management
- Service status monitoring
- Service scaling

### Docker Swarm
- Swarm cluster management
- Node management within the swarm
- Service deployment in swarm mode

### Cron Jobs
- Schedule management
- Job execution monitoring
- Job status reporting

## Architecture

The agent is built with a modular architecture:

### Controllers
- `NodeController`: Manages node registration and status
- `SystemController`: Monitors system resources
- `DockerController`: Handles Docker container operations
- `ComposeController`: Manages Docker Compose services
- `SwarmController`: Handles Docker Swarm operations
- `CronController`: Manages scheduled tasks

### Routes
- Handshake routes for node registration
- Docker operation routes
- Compose service routes
- Swarm management routes
- Cron job routes

### Middleware
- Signature verification for secure communication
- Request validation and authentication

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

### With Environment Variables
```bash
export CENTRAL_URL="http://your-central-server:8084"
export AGENT_PORT=8080
./persys-agent
```

### With Docker Compose
A sample docker-compose file is provided in `docker/docker-compose.yml`:

```bash
cd docker
# Build and run the agent in the background
sudo docker-compose up --build -d
```

- The agent will be available on port 8080 by default.
- The Docker socket is mounted for container management.
- You can configure `CENTRAL_URL` and `AGENT_PORT` in the compose file.

### As a systemd Service
You can install and manage Persys Agent as a systemd service:

1. Install the agent and service:
   ```bash
   sudo make install
   ```
2. The service will be installed as `persys-agent.service` and enabled to start on boot.
3. Configuration is managed via `/etc/persys-agent.env`.
4. Control the service with:
   ```bash
   sudo systemctl status persys-agent
   sudo systemctl restart persys-agent
   sudo systemctl stop persys-agent
   ```

### Uninstall
To remove the agent and service:
```bash
sudo make uninstall
```

## Security

The agent implements request signature verification to ensure secure communication with the central server. All API requests must be properly signed and authenticated.

## Error Handling

The agent implements robust error handling and retry mechanisms for:
- Node registration
- Heartbeat communication
- Docker operations
- Service management

## Monitoring

The agent provides:
- Regular heartbeat updates to the central server
- System resource monitoring
- Container and service status tracking
- Error logging and reporting

## License

GNU GENERAL PUBLIC LICENSE