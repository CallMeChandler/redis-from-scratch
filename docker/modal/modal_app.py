"""Deploy the all-in-one image (C++ primary + replica + dashboard) to Modal.

    uvx --python 3.12 --from modal modal deploy docker/modal/modal_app.py

Reuses docker/all-in-one/Dockerfile unchanged. Only the dashboard's port 3000
is exposed; the Redis ports stay bound to 127.0.0.1 inside the container.
Exactly one container runs at all times (min = max = 1), because the dataset,
the replica link and every WebSocket session live in that one process tree.
"""

import os
import signal
import socket
import subprocess
import threading
import time
from pathlib import Path

import modal

# Only meaningful on the deploying machine; inside the container this module
# lives at /root/modal_app.py and the image definition is not rebuilt.
REPO_ROOT = Path(__file__).resolve().parents[2] if modal.is_local() else Path("/")
DASHBOARD_PORT = 3000
COMMIT_INTERVAL_S = 15

image = modal.Image.from_dockerfile(
    REPO_ROOT / "docker/all-in-one/Dockerfile",
    context_dir=REPO_ROOT,
    # Modal's runtime needs Python; the node:22 runtime stage has none.
    add_python="3.12",
).run_commands(
    # Modal only mounts volumes on empty paths, but the Dockerfile pre-creates
    # /data/{primary,replica}. Remove them here; @modal.enter recreates them
    # inside the mounted volume.
    "rm -rf /data && mkdir /data",
).add_local_file(
    # Public deployment only: cap the primary at 64 MiB (max-memory 67108864).
    REPO_ROOT / "docker/modal/primary.conf",
    "/etc/redis-from-scratch/primary.conf",
    copy=True,
)

volume = modal.Volume.from_name("redis-from-scratch-data", create_if_missing=True)

app = modal.App("redis-from-scratch", image=image)


def wait_for_port(port: int, timeout_s: float) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=1):
                return
        except OSError:
            time.sleep(0.25)
    raise RuntimeError(f"port {port} did not open within {timeout_s}s")


@app.cls(
    volumes={"/data": volume},
    min_containers=1,
    max_containers=1,
    cpu=0.5,
    memory=1024,
    # Long-lived WebSockets (CLI, transactions, Pub/Sub) are single inputs.
    timeout=24 * 60 * 60,
)
@modal.concurrent(max_inputs=500)
class Dashboard:
    @modal.enter()
    def start(self) -> None:
        # The volume mount hides the directories the image created under /data.
        for name in ("primary", "replica"):
            (Path("/data") / name).mkdir(parents=True, exist_ok=True)

        # start-all (docker/all-in-one/start.sh) supervises all three processes
        # and forwards SIGTERM so the C++ servers write their shutdown snapshot.
        self.proc = subprocess.Popen(["start-all"], start_new_session=True)
        wait_for_port(DASHBOARD_PORT, 90)

        self.stopping = threading.Event()
        threading.Thread(target=self._commit_loop, daemon=True).start()
        threading.Thread(target=self._watchdog, daemon=True).start()

    def _commit_loop(self) -> None:
        # Modal only auto-commits on container exit; persist AOF/RDB regularly.
        while not self.stopping.wait(COMMIT_INTERVAL_S):
            try:
                volume.commit()
            except Exception as error:  # keep serving even if one commit fails
                print(f"volume commit failed: {error}", flush=True)

    def _watchdog(self) -> None:
        # If any server dies, start-all exits; fail the container so Modal replaces it.
        code = self.proc.wait()
        if not self.stopping.is_set():
            print(f"start-all exited with {code}; terminating container", flush=True)
            os._exit(1)

    @modal.web_server(DASHBOARD_PORT, startup_timeout=120, label="redis-from-scratch")
    def serve(self) -> None:
        # The server is already listening (started in @modal.enter).
        pass

    @modal.exit()
    def stop(self) -> None:
        self.stopping.set()
        if self.proc.poll() is None:
            # start-all forwards TERM to dashboard, replica and primary in turn.
            self.proc.terminate()
            try:
                self.proc.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)
        volume.commit()
