# Makefile for PersysAgent
# Builds, tests, and deploys the C++ Docker agent for non-Kubernetes workloads

# Variables
BINARY_NAME=PersysAgent
IMAGE_NAME=persys-agent
IMAGE_TAG=latest
BUILD_DIR=build
SRC_DIR=src

# Compiler and tools
CMAKE=cmake
MAKE=make
DOCKER=docker

# Default target
.PHONY: all
all: build docker-build

# Build the PersysAgent binary
.PHONY: build
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF .. && $(MAKE) -j$(shell nproc)
	@echo "Built $(BINARY_NAME) in $(BUILD_DIR)"

# Run the PersysAgent binary
.PHONY: run
run: build
	./$(BUILD_DIR)/$(BINARY_NAME)
	@echo "Ran $(BINARY_NAME)"

# Build the Docker image
.PHONY: docker-build
docker-build:
	$(DOCKER) build -t $(IMAGE_NAME):$(IMAGE_TAG) -f Dockerfile.ubuntu .
	@echo "Built Docker image $(IMAGE_NAME):$(IMAGE_TAG)"

# Run the PersysAgent container
.PHONY: docker-run
docker-run:
	$(DOCKER) run -d --name $(IMAGE_NAME) -p 8080:8080 -v /var/run/docker.sock:/var/run/docker.sock $(IMAGE_NAME):$(IMAGE_TAG)
	@echo "Started $(BINARY_NAME) container, API on localhost:8080"

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SYSTEMD_DIR ?= /etc/systemd/system
ENV_FILE ?= /etc/persys-agent.env
SERVICE_FILE ?= systemd/persys-agent.service

install: build
	install -Dm755 $(BINARY_NAME) $(BINDIR)/$(BINARY_NAME)
	@if ! id persys &>/dev/null; then \
		useradd --system --no-create-home --shell /usr/sbin/nologin persys; \
	fi
	@if [ ! -f $(ENV_FILE) ]; then \
		echo "AGENT_PORT=8080" > $(ENV_FILE); \
		echo "CENTRAL_URL=http://central.server:8084" >> $(ENV_FILE); \
		echo "Edit $(ENV_FILE) to configure the agent."; \
	fi
	install -Dm644 systemd/$(BINARY_NAME).service $(SYSTEMD_DIR)/$(BINARY_NAME).service
	systemctl daemon-reload
	systemctl enable $(BINARY_NAME)
	systemctl restart $(BINARY_NAME)
	@echo "Persys agent installed and started. Check status with: systemctl status $(BINARY_NAME)"

uninstall:
	systemctl stop $(BINARY_NAME) || true
	systemctl disable $(BINARY_NAME) || true
	rm -f $(BINDIR)/$(BINARY_NAME)
	rm -f $(SYSTEMD_DIR)/$(BINARY_NAME).service
	rm -f $(ENV_FILE)
	systemctl daemon-reload
	@echo "Persys agent uninstalled."

.PHONY: install uninstall

# Run tests (placeholder, update with actual tests)
.PHONY: test
test:
	@echo "Running tests (update with actual test commands)"
	# Add test commands here, e.g., unit tests or integration tests
	# Example: $(BUILD_DIR)/$(BINARY_NAME) --test

# Clean build artifacts
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts"

# Clean Docker image
.PHONY: clean-docker
clean-docker:
	$(DOCKER) rmi $(IMAGE_NAME):$(IMAGE_TAG) || true
	@echo "Removed Docker image $(IMAGE_NAME):$(IMAGE_TAG)"

# Full clean
.PHONY: clean-all
clean-all: clean clean-docker
	@echo "Cleaned all artifacts"

# Help
.PHONY: help
help:
	@echo "Makefile for PersysAgent"
	@echo "Targets:"
	@echo "  all            Build binary and Docker image"
	@echo "  install        Install PersysAgent Systemd Service"
	@echo "  uninstall      Uninstall PersysAgent Systemd Service"
	@echo "  build          Build PersysAgent binary"
	@echo "  run            Run PersysAgent binary"
	@echo "  docker-build   Build Docker image"
	@echo "  docker-run     Run PersysAgent container"
	@echo "  test           Run tests (placeholder)"
	@echo "  clean          Remove build artifacts"
	@echo "  clean-docker   Remove Docker image"
	@echo "  clean-all      Remove all artifacts"
	@echo "  help           Show this help"