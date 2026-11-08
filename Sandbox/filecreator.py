"""Create renderable BIN files compatible with QSpace Stars / BIN / V2.

File layout:
    int32   particle_count (little-endian)
    double  timestamp       (little-endian)
    repeated particle records, each containing 7 doubles:
        x, y, z, vx, vy, vz, mass

Generated positions form a deterministic three-dimensional cloud centered at
zero. The coordinate, velocity, and mass scales match the observed DM files.

Examples:
    python filecreator.py
    python filecreator.py --output-dir C:\\Users\\Zenbook\\NIR\\TF250\\bin\\generated
    python filecreator.py --sizes 1000 1000000particles 10000000particles
"""

from __future__ import annotations

import argparse
import math
import re
import struct
from pathlib import Path
from typing import Iterator

HEADER = struct.Struct("<id")
PARTICLE = struct.Struct("<7d")
BYTES_PER_PARTICLE = PARTICLE.size
DEFAULT_FILE_COUNT = 0
DEFAULT_MAX_PARTICLES = 100_000_000
DEFAULT_PARTICLE_COUNTS = (
    1,
    10,
    100,
    1_000,
    10_000,
    100_000,
    1_000_000,
    10_000_000,
    100_000_000,
)
POSITION_SCALE = 9.0
VELOCITY_SCALE = 3.0
MASS = 1.27780075073242e-05


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
    return max(1, int(number * multipliers[unit]))


def particle_count(target: int, is_particle_count: bool) -> int:
    if is_particle_count:
        return target
    if target <= HEADER.size:
        return 1
    return max(1, (target - HEADER.size) // BYTES_PER_PARTICLE)


def records(count: int, chunk_size: int, seed: int) -> Iterator[bytes]:
    """Yield spatially distributed records without retaining a whole file in memory."""
    azimuth_increment = 4.743698814490869
    for start in range(0, count, chunk_size):
        end = min(start + chunk_size, count)
        chunk = bytearray((end - start) * BYTES_PER_PARTICLE)
        for index in range(start, end):
            fraction = (index + 0.5) / count
            radius = POSITION_SCALE * fraction ** (1.0 / 3.0)
            z = 1.0 - 2.0 * ((index + seed) * 0.6180339887498949 % 1.0)
            radial = math.sqrt(max(0.0, 1.0 - z * z))
            angle = (index + seed) * azimuth_increment
            x = radius * radial * math.cos(angle)
            y = radius * radial * math.sin(angle)
            velocity_x = -VELOCITY_SCALE * y / POSITION_SCALE
            velocity_y = VELOCITY_SCALE * x / POSITION_SCALE
            PARTICLE.pack_into(
                chunk,
                (index - start) * BYTES_PER_PARTICLE,
                x,
                y,
                radius * z,
                velocity_x,
                velocity_y,
                0.0,
                MASS,
            )
        yield chunk


def create_file(path: Path, count: int, chunk_size: int, seed: int) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb", buffering=1024 * 1024) as output:
        output.write(HEADER.pack(count, 0.0))
        for chunk in records(count, chunk_size, seed):
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
        help="generate this many linearly spaced files from 1 to --max-particles",
    )
    parser.add_argument(
        "--max-particles",
        type=int,
        default=DEFAULT_MAX_PARTICLES,
        help="largest generated particle count (default: 100000000)",
    )
    parser.add_argument(
        "--sizes",
        nargs="+",
        help="explicit particle counts or byte sizes; overrides --file-count",
    )
    parser.add_argument(
        "--chunk-particles",
        type=int,
        default=65536,
        help="number of particle records packed per write (default: 65536)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=0,
        help="deterministic rotation of the generated particle cloud (default: 0)",
    )
    args = parser.parse_args()

    if args.chunk_particles < 1:
        parser.error("--chunk-particles must be positive")
    if args.file_count < 0:
        parser.error("--file-count must not be negative")
    if not 1 <= args.max_particles <= 2**31 - 1:
        parser.error("--max-particles must be between 1 and 2147483647")

    if args.sizes:
        targets = [(parse_size(raw_size), raw_size) for raw_size in args.sizes]
    elif args.file_count:
        targets = [
            (1 + (args.max_particles - 1) * index // args.file_count, "particles")
            for index in range(1, args.file_count + 1)
        ]
    else:
        targets = [(count, "particles") for count in DEFAULT_PARTICLE_COUNTS]

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for index, (target, raw_size) in enumerate(targets, start=1):
        is_particle_count = (
            raw_size.strip().lower().endswith(("particle", "particles"))
            or raw_size.strip().isdigit()
        )
        count = particle_count(target, is_particle_count)
        path = args.output_dir / f"stars_v2_{index:03d}_{count}_particles.bin"
        size = create_file(path, count, args.chunk_particles, args.seed)
        print(f"{path.name}: {count:,} particles, {size / (1024 ** 2):.2f} MiB")


if __name__ == "__main__":
    main()
