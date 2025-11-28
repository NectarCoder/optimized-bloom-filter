#!/usr/bin/env bash
# Sets up a Python virtual environment in the repository root and installs dependencies from this folder's requirements.txt
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
VENV_DIR="$REPO_ROOT/.venv"
REQ_FILE="$SCRIPT_DIR/requirements.txt"

if ! command -v python3 &> /dev/null; then
    echo "Error: python3 is not installed or not in PATH" >&2
    exit 1
fi

PYTHON_VERSION=$(python3 --version)
echo "Found: $PYTHON_VERSION"

if [ -d "$VENV_DIR" ]; then
    echo "Virtual environment already exists. Skipping creation."
else
    python3 -m venv "$VENV_DIR"
    echo "Virtual environment created successfully"
fi

source "$VENV_DIR/bin/activate"
python3 -m pip install --upgrade pip
pip install -r "$REQ_FILE"

echo "Setup complete! To activate the venv: source .venv/bin/activate"