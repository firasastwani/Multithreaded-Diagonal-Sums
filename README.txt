Firas Astwani (810123456)

Project 4: Multithreaded Diagonal Sums

Compilation and Running Instructions:
1. Make sure you have the following files in your directory:
   - proj4.c
   - proj4.h
   - main.c
   - Makefile
   - input files (in1.txt, in2.txt, etc.)


Generates new random input file
python generate_input.py 5 3 inputFiles/random_input.txt

2. To compile the program:
   make

3. To run the program:
   ./proj4.out inputFile outputFile s t
   where:
   - inputFile: name of the input file containing the grid
   - outputFile: name of the output file to write results
   - s: target sum to find
   - t: number of threads (1-3)

Performance Analysis:
I tested the program with various grid sizes and thread counts. Here are my findings:

1. Small grids (n < 100):
   - Single thread performance is often better than multi-threaded
   - Thread creation overhead outweighs parallelization benefits
   - Example: 5x5 grid with sum=10
     - 1 thread: ~0.000002s
     - 2 threads: ~0.000003s
     - 3 threads: ~0.000004s

2. Medium grids (100 <= n < 1000):
   - 2 threads show consistent improvement over single thread
   - 3 threads show marginal improvement over 2 threads
   - Example: 672x672 grid with sum=3
     - 1 thread: ~0.005s
     - 2 threads: ~0.003s
     - 3 threads: ~0.002s

3. Large grids (n >= 1000):
   - Multi-threading shows significant performance benefits
   - 3 threads consistently outperform 2 threads
   - Example: 2778x2778 grid with sum=13
     - 1 thread: ~0.4s
     - 2 threads: ~0.2s
     - 3 threads: ~0.15s

The program implements an O(n³) time complexity algorithm with O(n²) space complexity. The parallelization strategy divides the grid into roughly equal portions for each thread to process, with the main thread handling the last portion when using multiple threads. 