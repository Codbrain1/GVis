"""Create BIN files compatible with QSpace Stars / BIN / V2 standard scheme.

File layout:
    int32   particle_count (little-endian)
    double  timestamp       (little-endian)
    repeated particle records, each containing 7 doubles:
        x, y, z, vx, vy, vz, mass

Examples:
    python filecreator.py
    python filecreator.py --output-dir .\\stress-data --sizes 2 1MiB 100MiB 1GiB
"""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path
from typing import Iterable

HEADER = struct.Struct("<id")
PARTICLE = struct.Struct("<7d")
BYTES_PER_PARTICLE = PARTICLE.size
DEFAULT_FILE_COUNT = 100
DEFAULT_MAX_SIZE = "5GiB"


def parse_size(value: str) -> int:
    """Parse a particle count or a byte size such as 2, 1MiB, or 1GB."""
    match = re.fullmatch(
        r"(\d+(?:\.\d+)?)\s*([kmgt]?i?b|particles?)?", value.strip(), re.IGNORECASE
    )
    if not match:
        raise argparse.ArgumentTypeError(f"invalid size: {value!r}")

    number = float(match.group(1))
    unit = (match.group(2) or "particles").lower()
    multipliers = {
        "particles": 1,
        "particle": 1,
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
    if unit not in multipliers:
        raise argparse.ArgumentTypeError(f"unsupported size unit: {unit}")
    return max(
        2 if unit in {"particles", "particle"} else 1, int(number * multipliers[unit])
    )


def particle_count(target: int, is_particle_count: bool) -> int:
    if is_particle_count:
        return target
    if target <= HEADER.size:
        return 2
    return max(2, (target - HEADER.size) // BYTES_PER_PARTICLE)


def records(count: int, chunk_size: int) -> Iterable[bytes]:
    """Yield packed records without keeping the complete file in memory."""
    template = PARTICLE.pack(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0)
    for start in range(0, count, chunk_size):
        end = min(start + chunk_size, count)
        yield template * (end - start)


def create_file(path: Path, count: int, chunk_size: int) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb", buffering=1024 * 1024) as output:
        output.write(HEADER.pack(count, 0.0))
        for chunk in records(count, chunk_size):
            output.write(chunk)
    return path.stat().st_size


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create QSpace Stars V2 BIN stress-test files"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "stress-data",
        help="directory for generated .bin files",
    )
    parser.add_argument(
        "--file-count",
        type=int,
        default=DEFAULT_FILE_COUNT,
        help="number of files with linearly increasing sizes (default: 100)",
    )
    parser.add_argument(
        "--max-size",
        default=DEFAULT_MAX_SIZE,
        help="target size of the largest file, e.g. 10GiB (default: 10GiB)",
    )
    parser.add_argument(
        "--sizes",
        nargs="+",
        help="optional explicit particle counts or target sizes; overrides --file-count/--max-size",
    )
    parser.add_argument(
        "--chunk-particles",
        type=int,
        default=65536,
        help="number of particle records packed per write (default: 65536)",
    )
    args = parser.parse_args()

    if args.chunk_particles < 1:
        parser.error("--chunk-particles must be positive")
    if args.file_count < 1:
        parser.error("--file-count must be positive")

    if args.sizes:
        targets = [(parse_size(raw_size), raw_size) for raw_size in args.sizes]
    else:
        max_size = parse_size(args.max_size)
        if max_size <= HEADER.size:
            parser.error("--max-size must be larger than the BIN header")
        max_particles = max(2, (max_size - HEADER.size) // BYTES_PER_PARTICLE)
        targets = [
            (2 + (max_particles - 2) * index // args.file_count, "particles")
            for index in range(1, args.file_count + 1)
        ]

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for index, (target, raw_size) in enumerate(targets, start=1):
        is_particle_count = (
            raw_size.strip().lower().endswith(("particle", "particles"))
            or raw_size.strip().isdigit()
        )
        count = particle_count(target, is_particle_count)
        path = args.output_dir / f"stars_v2_{index:03d}_{count}_particles.bin"
        size = create_file(path, count, args.chunk_particles)
        print(f"{path.name}: {count:,} particles, {size / (1024 ** 2):.2f} MiB")


if __name__ == "__main__":
    main()
