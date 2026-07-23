#!/usr/bin/env python3
"""Download and freeze files declared in experiments/official_sources.json.

The downloader is deliberately independent of paper-author repositories.  A
completed file is accepted only after its size and SHA-256 have been computed;
interrupted transfers remain as ``.part`` files and are resumed when the HTTP
server supports byte ranges.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / "experiments" / "official_sources.json"
USER_AGENT = "ABHSS-official-benchmark-freezer/1.0 (+research artifact)"
CHUNK_BYTES = 8 * 1024 * 1024


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def digest_file(path: Path, algorithm: str = "sha256") -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as source:
        while chunk := source.read(CHUNK_BYTES):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as source:
        return json.load(source)


def write_json_atomic(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        json.dump(value, output, indent=2, sort_keys=True, ensure_ascii=False)
        output.write("\n")
    os.replace(temporary, path)


def verify_catalog_expectation(source_file: dict, byte_count: int, sha256: str) -> None:
    expected_bytes = source_file.get("expected_bytes")
    expected_sha256 = source_file.get("expected_sha256")
    if expected_bytes is not None and byte_count != expected_bytes:
        raise ValueError(
            f"upstream size mismatch: expected {expected_bytes}, received {byte_count}"
        )
    if expected_sha256 is not None and sha256.lower() != expected_sha256.lower():
        raise ValueError(
            "upstream SHA-256 mismatch: "
            f"expected {expected_sha256}, received {sha256}"
        )


def selected_datasets(catalog: dict, names: set[str], tiers: set[str]) -> list[dict]:
    selected = []
    for dataset in catalog["datasets"]:
        if dataset.get("status") not in {
            "include",
            "include_supplemental",
            "include_graph_query_hold",
        }:
            continue
        if names and dataset["id"] not in names:
            continue
        if tiers and dataset.get("tier") not in tiers:
            continue
        selected.append(dataset)
    missing = names - {dataset["id"] for dataset in selected}
    if missing:
        raise ValueError("Unknown or non-downloadable dataset(s): " + ", ".join(sorted(missing)))
    return selected


def headers_to_dict(response: object) -> dict[str, str | None]:
    headers = response.headers
    return {
        "content_type": headers.get("Content-Type"),
        "etag": headers.get("ETag"),
        "last_modified": headers.get("Last-Modified"),
        "accept_ranges": headers.get("Accept-Ranges"),
    }


def download_urllib(url: str, destination: Path, retries: int) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    for attempt in range(retries + 1):
        existing = partial.stat().st_size if partial.exists() else 0
        request_headers = {"User-Agent": USER_AGENT, "Accept-Encoding": "identity"}
        if existing:
            request_headers["Range"] = f"bytes={existing}-"
        request = urllib.request.Request(url, headers=request_headers)
        started = time.monotonic()
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                status = getattr(response, "status", response.getcode())
                response_headers = headers_to_dict(response)
                final_url = response.geturl()
                if existing and status != 206:
                    print(f"  server ignored Range; restarting {destination.name}", flush=True)
                    existing = 0
                    mode = "wb"
                else:
                    mode = "ab" if existing else "wb"
                announced = int(response.headers.get("Content-Length", "0") or 0)
                expected = existing + announced if announced else None
                transferred = existing
                next_report = transferred + 256 * 1024 * 1024
                with partial.open(mode) as output:
                    while True:
                        chunk = response.read(CHUNK_BYTES)
                        if not chunk:
                            break
                        output.write(chunk)
                        transferred += len(chunk)
                        if transferred >= next_report:
                            elapsed = max(time.monotonic() - started, 0.001)
                            rate = (transferred - existing) / elapsed / (1024 * 1024)
                            total_text = f"/{expected / (1024**3):.2f} GiB" if expected else ""
                            print(
                                f"  {transferred / (1024**3):.2f}{total_text} GiB "
                                f"({rate:.1f} MiB/s)",
                                flush=True,
                            )
                            next_report += 256 * 1024 * 1024
                actual = partial.stat().st_size
                if expected is not None and actual != expected:
                    raise IOError(f"short transfer: expected {expected} bytes, received {actual}")
                os.replace(partial, destination)
                return {
                    "requested_url": url,
                    "final_url": final_url,
                    "http_status": status,
                    **response_headers,
                    "bytes": destination.stat().st_size,
                    "sha256": digest_file(destination),
                    "retrieved_utc": utc_now(),
                }
        except (OSError, urllib.error.URLError, urllib.error.HTTPError) as error:
            if attempt >= retries:
                raise
            delay = min(2 ** attempt, 30)
            print(f"  retry {attempt + 1}/{retries} after {error}; waiting {delay}s", flush=True)
            time.sleep(delay)
    raise AssertionError("unreachable")


def parse_curl_headers(path: Path) -> dict[str, str | None]:
    """Return selected fields from the final response in a redirect chain."""
    blocks: list[dict[str, str]] = []
    current: dict[str, str] = {}
    with path.open("r", encoding="iso-8859-1", errors="replace") as source:
        for raw_line in source:
            line = raw_line.rstrip("\r\n")
            if line.startswith("HTTP/"):
                if current:
                    blocks.append(current)
                current = {":status": line}
            elif not line:
                if current:
                    blocks.append(current)
                    current = {}
            elif ":" in line and current:
                name, value = line.split(":", 1)
                current[name.strip().lower()] = value.strip()
    if current:
        blocks.append(current)
    final = blocks[-1] if blocks else {}
    return {
        "content_type": final.get("content-type"),
        "etag": final.get("etag"),
        "last_modified": final.get("last-modified"),
        "accept_ranges": final.get("accept-ranges"),
    }


def download_curl(url: str, destination: Path, retries: int) -> dict:
    """Download with Windows Schannel while retaining the same freeze record."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    header_path = destination.with_suffix(destination.suffix + ".headers.tmp")
    curl = shutil.which("curl.exe") or shutil.which("curl")
    if curl is None:
        raise RuntimeError("curl backend requested but curl was not found")
    command = [
        curl,
        "--location",
        "--fail",
        "--retry",
        str(retries),
        "--retry-all-errors",
        "--connect-timeout",
        "60",
        "--speed-limit",
        "1024",
        "--speed-time",
        "120",
        "--user-agent",
        USER_AGENT,
        "--continue-at",
        "-",
        "--output",
        str(partial),
        "--dump-header",
        str(header_path),
        "--write-out",
        "%{url_effective}\n%{http_code}\n%{size_download}\n",
        url,
    ]
    completed = subprocess.run(command, check=False, stdout=subprocess.PIPE, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"curl exited with status {completed.returncode}")
    fields = completed.stdout.splitlines()
    if len(fields) < 3:
        raise RuntimeError(f"unexpected curl metadata: {completed.stdout!r}")
    final_url, status_text, _downloaded_text = fields[-3:]
    response_headers = parse_curl_headers(header_path)
    os.replace(partial, destination)
    try:
        header_path.unlink()
    except FileNotFoundError:
        pass
    return {
        "requested_url": url,
        "final_url": final_url,
        "http_status": int(status_text),
        **response_headers,
        "bytes": destination.stat().st_size,
        "sha256": digest_file(destination),
        "retrieved_utc": utc_now(),
        "tls_backend": "curl-schannel" if os.name == "nt" else "curl",
    }


def download(url: str, destination: Path, retries: int, backend: str) -> dict:
    if backend == "auto":
        backend = "curl" if os.name == "nt" and shutil.which("curl.exe") else "urllib"
    if backend == "curl":
        return download_curl(url, destination, retries)
    return download_urllib(url, destination, retries)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--dataset", action="append", default=[], help="exact catalog dataset id; repeatable")
    parser.add_argument("--tier", action="append", choices=("core", "extended", "full"), default=[])
    parser.add_argument("--root", type=Path, help="override catalog download_root")
    parser.add_argument("--manifest", type=Path, help="override freeze download manifest")
    parser.add_argument("--retries", type=int, default=4)
    parser.add_argument("--backend", choices=("auto", "curl", "urllib"), default="auto")
    parser.add_argument("--rehash", action="store_true", help="rehash existing files instead of trusting matching manifest entries")
    args = parser.parse_args()

    catalog_path = args.catalog.resolve()
    catalog = load_json(catalog_path)
    raw_root = (args.root or (ROOT / catalog["download_root"])).resolve()
    freeze_root = raw_root.parent
    manifest_path = (args.manifest or (freeze_root / "download_manifest.json")).resolve()
    old_manifest = load_json(manifest_path) if manifest_path.exists() else {}
    old_by_path = {entry["path"]: entry for entry in old_manifest.get("files", [])}
    manifest = {
        "schema_version": 1,
        "freeze_id": catalog["freeze_id"],
        "catalog": str(catalog_path.relative_to(ROOT)).replace("\\", "/"),
        "raw_root": str(raw_root.relative_to(ROOT)).replace("\\", "/"),
        "updated_utc": utc_now(),
        "files": list(old_by_path.values()),
    }
    selected = selected_datasets(catalog, set(args.dataset), set(args.tier))
    if not selected:
        parser.error("selection contains no downloadable datasets")

    failures: list[tuple[str, str]] = []
    for dataset in selected:
        for source_file in dataset.get("files", []):
            relative = source_file["path"]
            destination = raw_root / relative
            prior = old_by_path.get(relative)
            if destination.exists() and prior and prior.get("bytes") == destination.stat().st_size:
                actual_sha256 = prior.get("sha256") if not args.rehash else digest_file(destination)
                if actual_sha256 == prior.get("sha256"):
                    verify_catalog_expectation(
                        source_file, destination.stat().st_size, actual_sha256
                    )
                    print(f"[{dataset['id']}] verified from manifest: {relative}", flush=True)
                    continue
            print(f"[{dataset['id']}] downloading {source_file['url']}", flush=True)
            try:
                record = download(source_file["url"], destination, args.retries, args.backend)
                if prior and (
                    record["bytes"] != prior.get("bytes")
                    or record["sha256"] != prior.get("sha256")
                ):
                    mismatch = destination.with_name(
                        destination.name + f".mismatch-{record['sha256'][:12]}"
                    )
                    os.replace(destination, mismatch)
                    raise RuntimeError(
                        "the official endpoint no longer serves the frozen bytes; "
                        f"saved the new payload as {mismatch.name} but did not alter the freeze"
                    )
                verify_catalog_expectation(source_file, record["bytes"], record["sha256"])
                record.update({"dataset": dataset["id"], "role": source_file["role"], "path": relative})
                old_by_path[relative] = record
                manifest["files"] = sorted(old_by_path.values(), key=lambda entry: entry["path"])
                manifest["updated_utc"] = utc_now()
                write_json_atomic(manifest_path, manifest)
                print(
                    f"  complete: {record['bytes']} bytes sha256={record['sha256']}",
                    flush=True,
                )
            except Exception as error:  # keep other independent sources downloadable
                failures.append((relative, str(error)))
                print(f"  FAILED: {error}", file=sys.stderr, flush=True)

    manifest["files"] = sorted(old_by_path.values(), key=lambda entry: entry["path"])
    manifest["updated_utc"] = utc_now()
    write_json_atomic(manifest_path, manifest)
    if failures:
        print("Download failures:", file=sys.stderr)
        for path, error in failures:
            print(f"  {path}: {error}", file=sys.stderr)
        return 1
    print(f"Frozen {sum(len(dataset.get('files', [])) for dataset in selected)} files in {raw_root}")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
