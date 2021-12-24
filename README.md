# CacheSimulation
Group project of implementing a cache table

milestone1.c is the C source file.
Command line args needed:
 -f [filename] -s [size of cache] -b [size of block] -a [associativity] -r [replacement policy "RR" or "RND"] -p [physicla mem size a number followed by GB, MB...]

For more information look at "2021 08-Cache_Simulator.pdf"

This creates a 2D array for the implementation of a cahce table. Reads a trace file "*.trc" that contains addresses. Gets the offset, index, and tag bits from
given address, looks for it in the cache table and adds it if was not found. If the table is full, a replacement policy will be used to write into the table.

Files provided are multiple trace files, the C source file, a report excel running the program with various different command line arguments, and the project assignment.
