## Changelog

### 0.1.2 - 28.09.2023:

Changes in current version:

* fix bug in reciprocal generation and calculation for Superscalar program
* fix copy bug in hasher reset
* correct original randomx dataset benchmark result (it was calculated incorrectly and is slightly higher now)
* add few new testcases
* rename superscalar program related structures with `Superscalar` prefix to not conflict with random program related structures
* documentation corrections

### 0.1.1 - 07.09.2023:

Some cleanup and improvements:

* increase benchmark time from 10 to 60 seconds
* change some benchmarks to calculate hashes per second instead of megabytes per second and added benchmark for measuring efficiency of the algorithm
* add some test cases to increase code coverage (not measured yet)
* change alias for XMM registers in sse.hpp to use __m128d instead of std::array
* clean up some code in utils
* clean up some code in blake2b and argon2d
* clean up projects properties

### 0.1.0 - 03.09.2023:

Reference implementation of the project.
Later optimization and improvements will base on this version.
All of the code was meant to be tested, documented and followed coding guidelines.

### 0.0.1 - 10.08.2023:

Initial implementation of the project.
RandomX hashes are calculated correctly, but no optimizatons are included.
Most of the code is tested, documented and follows coding guidelines.
