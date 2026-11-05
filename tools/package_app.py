#!/usr/bin/env python3
"""Package GraphicsApp sources from the reviewed, explicit source list."""
import argparse
import os
from pathlib import Path
from app_package import package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parents[1] / 'build')
    args = parser.parse_args()
    try:
        print(package(Path(__file__).resolve().parents[1], args.output_dir.resolve(),
                      int(os.environ.get('SOURCE_DATE_EPOCH', '0'))))
    except (OSError, ValueError) as error:
        parser.exit(1, f'Package failed: {error}\n')


if __name__ == '__main__':
    main()
