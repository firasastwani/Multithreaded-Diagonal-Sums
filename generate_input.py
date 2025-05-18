#!/usr/bin/env python3
import random
import sys
import os

def generate_input_file(rows, cols, output_file):
    """
    Generate a random input file with the specified dimensions.
    Each number will be a 5-digit number (00000-99999).
    """
    with open(output_file, 'w') as f:
        for _ in range(rows):
            # Generate a row of random 5-digit numbers
            row = [f"{random.randint(0, 99999):05d}" for _ in range(cols)]
            f.write(''.join(row) + '\n')

def main():
    if len(sys.argv) != 4:
        print("Usage: python generate_input.py <rows> <cols> <output_file>")
        print("Example: python generate_input.py 5 3 inputFiles/random_input.txt")
        sys.exit(1)

    try:
        rows = int(sys.argv[1])
        cols = int(sys.argv[2])
        output_file = sys.argv[3]
    except ValueError:
        print("Error: rows and cols must be integers")
        sys.exit(1)

    # Create output directory if it doesn't exist
    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    generate_input_file(rows, cols, output_file)
    print(f"Generated input file with {rows} rows and {cols} columns at {output_file}")

if __name__ == "__main__":
    main() 