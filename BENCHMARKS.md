# modernRX - Benchmarks

Benchmarks compare modernRX implementation with fully optimized RandomX implementation and with RandomX implementation that match optimizations available in current modernRX version.

Original RandomX provides benchmark only for calculating final hashes. All other values were estimated (based on information benchmark provides) or some custom benchmarks were written on top of original RandomX implementation, thus values may not be 100% accurate.

## Run benchmarks

To run benchmarks open solution set `benchmarks` project with `Release` or `ReleaseTrace` configuration as the startup one and click "run". `ReleaseTrace` configuration adds additional tracing code of particular hash calculation steps.
Several options are also available:
* `--seconds` - number of seconds to run benchmark <15-7200> (default: `Release: 60` | `ReleaseTrace: 300`)
* `--warmup` - number of seconds excluded from benchmark results <0-15> (default: 5)
* `--verbose` - set verbosity level <0-2> (default: 1)
* `--no-microbenchmarks` - disable microbenchmarks (default: `Release: false` | `ReleaseTrace: true`)

## AMD Ryzen 7840HS

RAM: 32GB (4x8GB, dual-channel, 6400 MT/s, DDR5)

System: Windows 11 Home

### Base clock (boost disabled - 3.8Ghz)

|                                      | Hash [H/s] | Efficiency [H/Watt/s] | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Superscalar [Prog/s] | Dataset [MB/s] |
| ------------------------------------ | :--------: | :-------------------: | :-----------: | :---------------: | :------------: | :------------------: | :------------: |
| RandomX-1.2.1 (102f8acf)             |   **3753** |            **~87.27** |        4.098M |           131.76K |          884.9 |                 5921 |         ~932.4 |
| RandomX-1.2.1 (102f8acf)<sup>7</sup> |       3068 |                ~77.67 |        4.098M |           131.76K |          884.9 |                 5921 |         ~859.2 |
| modernRX 0.8.2                       |       3070 |                ~77.72 |    **6.461M** |       **205.22K** |     **1223.9** |            **12934** |     **1437.7** |
|||||||||
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |       25.7 |                 ~1.51 |        4.098M |           131.76K |          426.0 |                 5921 |            3.4 |
| modernRX 0.1.4 (reference)           |       40.4 |                 ~2.29 |        3.296M |           107.71K |          465.5 |                11754 |            7.0 |


### Boost clock (up to 5.1Ghz)

|                                      | Hash [H/s] | Efficiency [H/Watt/s] | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Superscalar [Prog/s] | Dataset [MB/s] |
| ------------------------------------ | :--------: | :-------------------: | :-----------: | :---------------: | :------------: | :------------------: | :------------: |
| RandomX-1.2.1 (102f8acf)             |   **4020** |            **~58.00** |        5.424M |           173.40K |         1036.1 |                 7660 |        ~1046.3 |
| RandomX-1.2.1 (102f8acf)<sup>7</sup> |       3260 |                ~46.90 |        5.424M |           173.40K |         1036.1 |                 7660 |         ~911.5 |
| modernRX 0.8.2                       |       3294 |                ~47.39 |    **8.248M** |       **268.26K** |     **1368.6** |            **16895** |     **1437.0** |
|||||||||
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |       34.2 |                 ~0.87 |        5.424M |           173.40K |          530.8 |                 7660 |            4.5 |
| modernRX 0.1.4 (reference)           |       50.9 |                 ~1.30 |        4.351M |           141.63K |          597.6 |                15562 |            9.6 |


## AMD Ryzen 5800h

RAM: 32GB (2x16GB, dual-channel, 3200 MT/s, DDR4)

System: Windows 11

### Base clock (boost disabled - 3.2Ghz, 95°C temp limit)

|                                      | Hash [H/s] | Efficiency [H/Watt/s] | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Superscalar [Prog/s] | Dataset [MB/s] |
| ------------------------------------ | :--------: | :-------------------: | :-----------: | :---------------: | :------------: | :------------------: | :------------: |
| RandomX-1.2.1 (102f8acf)             |   **4554** |            **~84.33** |        3.231M |           103.46K |          881.4 |                 2754 |         ~838.7 |
| RandomX-1.2.1 (102f8acf)<sup>7</sup> |       3139 |                ~64.45 |        3.231M |           103.46K |          881.4 |                 2754 |         ~838.7 |
| modernRX 0.8.1                       |       3105 |                ~64.28 |        5.454M |           170.66K |         1236.2 |                 9647 |         1263.0 |
| modernRX 0.8.0                       |       3105 |                ~63.62 |        5.455M |           172.01K |         1217.6 |                 9690 |         1246.1 |
| modernRX 0.7.2                       |       3113 |                ~64.05 |        5.455M |           172.07K |         1238.7 |                 9619 |     **1268.0** |
| modernRX 0.7.1                       |       3043 |                ~62.74 |        5.449M |       **172.14K** |         1234.4 |                 9698 |         1247.6 |
| RandomX-1.2.1 (102f8acf)<sup>6</sup> |       2906 |                ~61.05 |        3.231M |           103.46K |          881.4 |                 2754 |         ~838.7 |
| modernRX 0.7.0                       |       2863 |                ~59.89 |        5.449M |           171.25K |         1235.4 |                 9093 |         1261.1 |
| RandomX-1.2.1 (102f8acf)<sup>5</sup> |      490.8 |                ~22.31 |        3.231M |           103.46K |          881.4 |                 2754 |         ~838.7 |
| modernRX 0.6.3                       |      496.4 |                ~22.26 |    **5.458M** |           169.61K |         1208.1 |                 9631 |         1238.4 |
| modernRX 0.6.2                       |      474.7 |                ~21.38 |        5.453M |           171.76K |         1239.9 |                 9623 |         1264.2 |
| modernRX 0.6.1                       |      340.6 |                ~14.49 |        5.457M |           171.78K |         1235.7 |                 9687 |         1267.1 |
| modernRX 0.6.0                       |      312.2 |                ~13.28 |        5.456M |           171.99K |         1215.1 |                 9652 |         1238.3 |
| RandomX-1.2.1 (102f8acf)<sup>4</sup> |       19.4 |                 ~0.62 |        3.231M |           103.46K |          881.4 |                 2754 |         ~838.7 |
| modernRX 0.5.1                       |       69.1 |                 ~2.82 |        5.457M |           171.99K |         1210.8 |                 9640 |         1253.2 |
| modernRX 0.5.0                       |       33.3 |                 ~1.11 |        5.450M |           171.11K |         1228.6 |                 9637 |         1236.4 |
| RandomX-1.2.1 (102f8acf)<sup>3</sup> |       18.6 |                 ~0.84 |        3.231M |           103.46K |          881.4 |                 2754 |         ~838.7 |
| modernRX 0.4.0                       |       31.6 |                 ~1.31 |        5.450M |           170.25K |     **1241.6** |             **9741** |         1254.0 |
| modernRX 0.3.12                      |       26.3 |                 ~0.89 |        5.450M |           170.98K |         1216.9 |                 9671 |         1260.1 |
| modernRX 0.3.11                      |       26.1 |                 ~0.87 |        5.001M |           158.60K |         1224.3 |                 9473 |         1262.0 |
| modernRX 0.3.10                      |       26.6 |                 ~0.91 |        4.993M |           159.09K |         1231.5 |                 9459 |         1242.7 |
| modernRX 0.3.9                       |       25.5 |                 ~0.85 |        4.994M |           159.01K |          977.6 |                 9566 |         1238.0 |
| modernRX 0.3.8                       |       26.2 |                 ~0.87 |        4.980M |           159.19K |          973.1 |                 9559 |         1179.3 |
| modernRX 0.3.7                       |       25.4 |                 ~0.85 |        4.985M |           158.94K |          948.3 |                 9509 |         1023.3 |
| modernRX 0.3.6                       |       25.7 |                 ~0.88 |        4.914M |           156.10K |          959.7 |                 9376 |         1006.2 |
| modernRX 0.3.5                       |       26.7 |                 ~0.90 |        4.946M |           155.74K |          943.9 |                 9340 |          987.6 |
| modernRX 0.3.4                       |       26.8 |                 ~0.92 |        4.910M |           155.95K |          957.1 |                 9327 |         1014.7 |
| modernRX 0.3.3                       |       27.0 |                 ~0.93 |        4.911M |           155.90K |          952.8 |                 9270 |          926.0 |
| modernRX 0.3.2                       |       25.6 |                 ~0.85 |        4.901M |           154.91K |          954.9 |                 9307 |          934.2 |
| modernRX 0.3.1                       |       26.2 |                 ~0.90 |        4.867M |           156.60K |          958.7 |                 9355 |          911.0 |
| modernRX 0.3.0                       |       26.9 |                 ~0.92 |        4.906M |           156.62K |          957.6 |                 9350 |          889.4 |
| RandomX-1.2.1 (102f8acf)<sup>2</sup> |       18.6 |                 ~0.84 |        3.231M |           103.46K |          881.4 |                 2754 |            --- |
| modernRX 0.2.3                       |       26.2 |                 ~0.87 |        4.872M |           154.30K |          912.7 |                 9356 |          107.8 |
| modernRX 0.2.2                       |       25.8 |                 ~0.83 |        4.893M |           156.04K |          942.2 |                 9419 |           36.9 |
| modernRX 0.2.1                       |       25.9 |                 ~0.89 |        4.903M |           156.11K |          927.9 |                 9458 |           34.3 |
| modernRX 0.2.0                       |       25.8 |                 ~0.86 |        4.902M |           154.70K |          944.4 |                 9409 |           18.3 |
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |       18.6 |                 ~0.84 |        3.231M |           103.46K |          386.7 |                 2754 |            1.9 |
| modernRX 0.1.3 (reference)           |       32.4 |                 ~1.32 |        2.130M |            69.51K |          392.1 |                 8317 |            2.0 |


### Boost clock (up to 4.4Ghz, 95°C temp limit)

|                                      | Hash [H/s] | Efficiency [H/Watt/s] | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Superscalar [Prog/s] | Dataset [MB/s] |
| ------------------------------------ | :--------: | :-------------------: | :-----------: | :---------------: | :------------: | :------------------: | :------------: |
| RandomX-1.2.1 (102f8acf)             |   **5233** |            **~59.46** |        4.434M |           133.57K |         1089.3 |                 5750 |        ~1020.8 |
| RandomX-1.2.1 (102f8acf)<sup>7</sup> |       3428 |                ~44.34 |        4.434M |           133.57K |         1089.3 |                 5750 |        ~1020.8 |
| modernRX 0.8.1                       |       3325 |                ~43.46 |        7.569M |           238.76K |         1443.5 |                13169 |         1334.8 |
| modernRX 0.8.0                       |       3298 |                ~43.39 |        7.577M |           238.35K |         1462.3 |                13221 |         1351.7 |
| modernRX 0.7.2                       |       3363 |                ~43.96 |        7.544M |           237.68K |         1444.5 |                13140 |         1360.0 |
| modernRX 0.7.1                       |       3313 |                ~43.59 |        7.563M |           237.46K |         1422.3 |                13231 |         1310.6 |
| RandomX-1.2.1 (102f8acf)<sup>6</sup> |       3187 |                ~42.49 |        4.434M |           133.57K |         1089.3 |                 5750 |        ~1020.8 |
| modernRX 0.7.0                       |       3181 |                ~43.57 |        7.506M |           237.90K |	      1443.4 |                13232 |         1358.8 |
| RandomX-1.2.1 (102f8acf)<sup>5</sup> |      604.8 |                ~16.34 |        4.434M |           133.57K |         1089.3 |                 5750 |        ~1020.8 |
| modernRX 0.6.3                       |      603.6 |                ~16.44 |        7.577M |       **239.14K** |	      1426.3 |                13244 |         1356.4 |
| modernRX 0.6.2                       |      602.3 |                ~16.19 |    **7.595M** |           239.10K |     **1471.6** |            **13275** |         1364.9 |
| modernRX 0.6.1                       |      449.0 |                ~11.82 |        7.579M |           238.78K |         1444.8 |                12872 |         1356.7 |
| modernRX 0.6.0                       |      397.7 |                ~10.33 |        7.574M |           238.33K |         1456.9 |                13238 |         1365.6 |
| RandomX-1.2.1 (102f8acf)<sup>4</sup> |       27.4 |                 ~0.61 |        4.434M |           133.57K |         1089.3 |                 5750 |        ~1020.8 |
| modernRX 0.5.1                       |       92.7 |                 ~2.35 |        7.552M |           237.47K |         1413.6 |                13123 |         1353.4 |
| modernRX 0.5.0                       |       44.1 |                 ~0.94 |        7.486M |           236.77K |         1432.3 |                13095 |         1349.1 |
| RandomX-1.2.1 (102f8acf)<sup>3</sup> |        --- |                   --- |        4.434M |           133.57K |         1089.3 |                 5750 |        ~1020.8 |
| modernRX 0.4.0                       |       41.9 |                 ~1.06 |        7.552M |           237.58K |         1459.0 |                13244 |     **1370.9** |
| modernRX 0.3.12                      |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.11                      |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.10                      |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.9                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.8                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.7                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.6                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.5                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.4                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.3                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.2                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.1                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.3.0                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| RandomX-1.2.1 (102f8acf)<sup>2</sup> |        --- |                   --- |        4.434M |           133.57K |         1089.3 |                 5750 |            --- |
| modernRX 0.2.3                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.2.2                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.2.1                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| modernRX 0.2.0                       |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |        --- |                   --- |        4.434M |           133.57K |          514.0 |                 5750 |            --- |
| modernRX 0.1.3 (reference)           |        --- |                   --- |           --- |               --- |            --- |                  --- |            --- |


## Description

Benchmarks:
* Blake2b - calculating 64-byte blake2b hash for 64 bytes of input data.
* Blake2bLong - calculating 1024-byte blake2b hash for 72 bytes of input data.
* Argon2d - filling 256 MB of memory.
* Aes1R - generating 2MB of output data with 64 bytes of input data.
* Aes4R - generating 2176 bytes of output data with 64 bytes of input data.
* AesHash1R - calculating 64-byte aes hash for 2MB of input data.
* Superscalar - generating superscalar program.
* Dataset - generating >2GB of dataset.
* Hash - calculating final RandomX hash.
* Efficiency - calculating final RandomX hash per watt per second. Power consumption was measured by wattmeter.

Units:
* H/s - hashes per second
* MB/s - megabytes (1'048'576 bytes) per second
* Prog/s - programs per second
* H/Watt/s - hashes per watt per second

<sup>1)</sup> no avx argon2d, interpreted mode, software AES mode, small pages mode, no batch, single-threaded, full memory mode

<sup>2)</sup> avx2 argon2d, interpreted mode, software AES mode, small pages mode, no batch, multi-threaded dataset, single-threaded hash, full memory mode

<sup>3)</sup> avx2 argon2d, multi-threaded dataset JIT mode, single-threaded hash calculation interpreted mode, software AES mode, small pages mode, no batch, full memory mode

<sup>4)</sup> avx2 argon2d, multi-threaded dataset JIT mode, single-threaded hash calculation interpreted mode, hardware AES mode, small pages mode, no batch, full memory mode

<sup>5)</sup> avx2 argon2d, multi-threaded dataset JIT mode, single-threaded "insecure" hash calculation JIT mode, hardware AES mode, small pages mode, no batch, full memory mode

<sup>6)</sup> avx2 argon2d, multi-threaded dataset JIT mode, multi-threaded "insecure" hash calculation JIT mode, hardware AES mode, small pages mode, no batch, full memory mode

<sup>7)</sup> avx2 argon2d, multi-threaded dataset JIT mode, multi-threaded "insecure" hash calculation JIT mode, hardware AES mode, small pages mode, batch mode, thread affinity (21845), full memory mode
