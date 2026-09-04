#!/usr/bin/env python3
"""Verify opt-in metrics stay separate from output and identify VM execution."""
import json
import pathlib
import subprocess
import sys
import tempfile


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="kyna-metrics-") as directory:
        metrics = pathlib.Path(directory) / "metrics.json"
        result = subprocess.run(
            [str(binary), "run", "-", "--no-color", "--metrics-file", str(metrics)],
            input="fn twice(x: int): int { return x * 2; } print(map([21], twice)[0]);",
            capture_output=True, text=True, check=True)
        assert result.stdout == "42\n", result.stdout
        assert result.stderr == "", result.stderr
        report = json.loads(metrics.read_text())
        assert report["schema"] == "kyna.metrics/v1"
        assert report["executed"] is True
        phases = {item["phase"] for item in report["phases"]}
        assert {"lex", "parse", "check", "hir", "mir", "bytecode", "vm_execute"} <= phases
        assert "tree_execute" not in phases
        assert all(item["nanoseconds"] >= 0 for item in report["phases"])
        failure = subprocess.run(
            [str(binary), "check", "-", "--no-color", "--metrics-file", str(metrics)],
            input="var broken = ;", capture_output=True, text=True)
        assert failure.returncode != 0
        assert failure.stdout == ""
        report = json.loads(metrics.read_text())
        assert report["executed"] is False
        assert {item["phase"] for item in report["phases"]} == {"lex", "parse"}
    print("phase metrics verification passed")


if __name__ == "__main__":
    main()
