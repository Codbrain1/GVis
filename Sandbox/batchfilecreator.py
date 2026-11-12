"""Create equal-size BIN files compatible with GVis Stars / BIN / V2.

Each file contains:
    int32   particle_count (little-endian)
    double  timestamp      (little-endian)
    repeated records of seven doubles:
        x, y, z, vx, vy, vz, mass

The requested size is rounded down to a valid whole-particle size. The difference
is always less than one 56-byte particle record.

Examples:
    python batchfilecreator.py --file-size 28MiB --file-count 142
    python batchfilecreator.py --output-dir .\\Data\\BatchFileSequence\\generated --file-size 20MB --file-count 50
"""

from __future__ import annotations

import argparse
import re
import struct
from decimal import Decimal, InvalidOperation
from pathlib import Path

HEADER = struct.Struct("<id")
PARTICLE = struct.Struct("<7d")
BYTES_PER_PARTICLE = PARTICLE.size
DEFAULT_FILE_COUNT = 142
DEFAULT_FILE_SIZE = "80MiB"
MAX_PARTICLE_COUNT = 2**31 - 1


def parse_size(value: str) -> int:
    """Parse a byte size with optional B, KB, MB, GB, KiB, MiB, or GiB suffix."""
    match = re.fullmatch(r"\s*(\d+(?:\.\d+)?)\s*([kmgt]?i?b)?\s*", value, re.IGNORECASE)
    if not match:
        raise argparse.ArgumentTypeError(
            "size must look like 20MB, 28MiB, or 29360140B"
        )

    multipliers = {
        "b": 1,
        "kb": 1000,
        "mb": 1000**2,
        "gb": 1000**3,
        "tb": 1000**4,
        "kib": 1024,
        "mib": 1024**2,
        "gib": 1024**3,
        "tib": 1024**4,
    }
    unit = (match.group(2) or "b").lower()
    try:
        size = int(Decimal(match.group(1)) * multipliers[unit])
    except (InvalidOperation, KeyError) as error:
        raise argparse.ArgumentTypeError(f"invalid size: {value!r}") from error

    if size < HEADER.size + BYTES_PER_PARTICLE:
        raise argparse.ArgumentTypeError(
            f"size must be at least {HEADER.size + BYTES_PER_PARTICLE} bytes"
        )
    return size


def particle_count_for_size(target_size: int) -> int:
    return (target_size - HEADER.size) // BYTES_PER_PARTICLE


def write_file(
    path: Path, particle_count: int, chunk_particles: int, timestamp: float
) -> int:
    record = PARTICLE.pack(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0)
    full_chunk = record * chunk_particles

    with path.open("xb", buffering=1024 * 1024) as output:
        output.write(HEADER.pack(particle_count, timestamp))
        remaining = particle_count
        while remaining >= chunk_particles:
            output.write(full_chunk)
            remaining -= chunk_particles
        if remaining:
            output.write(record * remaining)

    return path.stat().st_size


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create equal-size GVis Stars/BIN/V2 stress-test files."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent
        / "Data"
        / "BatchFileSequence"
        / "generated",
        help="destination directory (default: Sandbox/Data/BatchFileSequence/generated)",
    )
    parser.add_argument(
        "--file-size",
        type=parse_size,
        default=parse_size(DEFAULT_FILE_SIZE),
        help=f"target size for every file (default: {DEFAULT_FILE_SIZE})",
    )
    parser.add_argument(
        "--file-count",
        type=int,
        default=DEFAULT_FILE_COUNT,
        help=f"number of files to create (default: {DEFAULT_FILE_COUNT})",
    )
    parser.add_argument(
        "--start-index",
        type=int,
        default=0,
        help="first numeric file index (default: 0)",
    )
    parser.add_argument(
        "--prefix",
        default="S_",
        help="file-name prefix (default: S_)",
    )
    parser.add_argument(
        "--chunk-particles",
        type=int,
        default=65_536,
        help="records written per buffered chunk (default: 65536)",
    )
    parser.add_argument(
        "--timestamp",
        type=float,
        default=0.0,
        help="timestamp written to each file header (default: 0.0)",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace existing generated files",
    )
    args = parser.parse_args()

    if args.file_count < 1:
        parser.error("--file-count must be positive")
    if args.chunk_particles < 1:
        parser.error("--chunk-particles must be positive")

    particle_count = particle_count_for_size(args.file_size)
    if particle_count > MAX_PARTICLE_COUNT:
        parser.error("--file-size exceeds the int32 particle-count limit")

    actual_size = HEADER.size + particle_count * BYTES_PER_PARTICLE
    paths = [
        args.output_dir / f"{args.prefix}{index:5d}.bin"
        for index in range(args.start_index, args.start_index + args.file_count)
    ]
    existing = [path for path in paths if path.exists()]
    if existing and not args.overwrite:
        parser.error(
            f"{len(existing)} target file(s) already exist; use --overwrite to replace them"
        )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    if args.overwrite:
        for path in existing:
            path.unlink()

    print(
        f"Creating {args.file_count} file(s): {particle_count:,} particles, "
        f"{actual_size:,} bytes ({actual_size / 1024**2:.2f} MiB) each."
    )
    if actual_size != args.file_size:
        print(
            f"Requested {args.file_size:,} bytes; rounded down by "
            f"{args.file_size - actual_size} bytes for whole records."
        )

    for position, path in enumerate(paths, start=1):
        written = write_file(path, particle_count, args.chunk_particles, args.timestamp)
        print(f"[{position}/{args.file_count}] {path.name}: {written:,} bytes")


if __name__ == "__main__":
    main()
