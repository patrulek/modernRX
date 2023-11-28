# modernRX - Benchmarks

Benchmarks compare modernRX implementation with fully optimized RandomX implementation and with RandomX implementation that match optimizations available in current modernRX version.

Original RandomX provides benchmark only for calculating final hashes. All other values were estimated (based on information benchmark provides) or some custom benchmarks were written on top of original RandomX implementation, thus values may not be 100% accurate.


## AMD Ryzen 5800h

RAM: 32GB (Dual-channel, 3200 MT/s)

System: Windows 11

### Base clock (boost disabled - 3.2Ghz, 95°C temp limit)

|                                      | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Aes1R [MB/s] | Aes4R [MB/s] | AesHash1R [H/s] | Superscalar [Prog/s] | Dataset [MB/s] | Hash [H/s] | Efficiency [H/Watt/s] |
| ------------------------------------ | :-----------: | :---------------: | :------------: | :----------: | :----------: | :-------------: | :------------------: | :------------: | :--------: | :-------------------: |
| RandomX-1.2.1 (102f8acf)             |        3.231M |           103.46K |          881.4 |      47402.5 |      11473.4 |           23702 |                 2754 |         ~838.7 |   **4554** |            **~84.33** |
| RandomX-1.2.1 (102f8acf)<sup>7</sup> |        3.231M |           103.46K |          881.4 |      47402.5 |      11473.4 |           23702 |                 2754 |         ~838.7 |       3139 |                ~64.45 |
| modernRX 0.7.1                       |        5.449M |       **172.14K** |         1234.4 |  **47465.2** |      11882.1 |           23725 |                 9698 |         1247.6 |       3043 |                ~62.74 |
| RandomX-1.2.1 (102f8acf)<sup>6</sup> |        3.231M |           103.46K |          881.4 |      47402.5 |      11473.4 |           23702 |                 2754 |         ~838.7 |       2906 |                ~61.05 |
| modernRX 0.7.0                       |        5.449M |           171.25K |         1235.4 |      47351.1 |      11882.9 |       **23815** |                 9093 |         1261.1 |       2863 |                ~59.89 |
| RandomX-1.2.1 (102f8acf)<sup>5</sup> |        3.231M |           103.46K |          881.4 |      47402.5 |      11473.4 |           23702 |                 2754 |         ~838.7 |      490.8 |                ~22.31 |
| modernRX 0.6.3                       |    **5.458M** |           169.61K |         1208.1 |      47430.5 |  **11890.3** |           23636 |                 9631 |         1238.4 |      496.4 |                ~22.26 |
| modernRX 0.6.2                       |        5.453M |           171.76K |         1239.9 |      47324.0 |      11862.1 |           23699 |                 9623 |         1264.2 |      474.7 |                ~21.38 |
| modernRX 0.6.1                       |        5.457M |           171.78K |         1235.7 |      47359.7 |      11864.3 |           23718 |                 9687 |     **1267.1** |      340.6 |                ~14.49 |
| modernRX 0.6.0                       |        5.456M |           171.99K |         1215.1 |      47271.9 |      11868.4 |           23743 |                 9652 |         1238.3 |      312.2 |                ~13.28 |
| RandomX-1.2.1 (102f8acf)<sup>4</sup> |        3.231M |           103.46K |          881.4 |      47402.5 |      11473.4 |           23702 |                 2754 |         ~838.7 |       19.4 |                 ~0.62 |
| modernRX 0.5.1                       |        5.457M |           171.99K |         1210.8 |      47119.6 |      11862.2 |           23640 |                 9640 |         1253.2 |       69.1 |                 ~2.82 |
| modernRX 0.5.0                       |        5.450M |           171.11K |         1228.6 |      47018.1 |      11847.7 |           23682 |                 9637 |         1236.4 |       33.3 |                 ~1.11 |
| RandomX-1.2.1 (102f8acf)<sup>3</sup> |        3.231M |           103.46K |          881.4 |       2333.3 |        524.7 |            1166 |                 2754 |         ~838.7 |       18.6 |                 ~0.84 |
| modernRX 0.4.0                       |        5.450M |           170.25K |     **1241.6** |       2781.1 |        728.6 |            1449 |             **9741** |         1254.0 |       31.6 |                 ~1.31 |
| modernRX 0.3.12                      |        5.450M |           170.98K |         1216.9 |       2805.5 |        723.2 |            1428 |                 9671 |         1260.1 |       26.3 |                 ~0.89 |
| modernRX 0.3.11                      |        5.001M |           158.60K |         1224.3 |       2770.8 |        724.6 |            1428 |                 9473 |         1262.0 |       26.1 |                 ~0.87 |
| modernRX 0.3.10                      |        4.993M |           159.09K |         1231.5 |       2776.8 |        723.5 |            1429 |                 9459 |         1242.7 |       26.6 |                 ~0.91 |
| modernRX 0.3.9                       |        4.994M |           159.01K |          977.6 |       2662.8 |        718.6 |            1420 |                 9566 |         1238.0 |       25.5 |                 ~0.85 |
| modernRX 0.3.8                       |        4.980M |           159.19K |          973.1 |       2790.1 |        685.4 |            1426 |                 9559 |         1179.3 |       26.2 |                 ~0.87 |
| modernRX 0.3.7                       |        4.985M |           158.94K |          948.3 |       2755.7 |        713.4 |            1410 |                 9509 |         1023.3 |       25.4 |                 ~0.85 |
| modernRX 0.3.6                       |        4.914M |           156.10K |          959.7 |       2635.3 |        700.5 |            1429 |                 9376 |         1006.2 |       25.7 |                 ~0.88 |
| modernRX 0.3.5                       |        4.946M |           155.74K |          943.9 |       2720.4 |        717.1 |            1424 |                 9340 |          987.6 |       26.7 |                 ~0.90 |
| modernRX 0.3.4                       |        4.910M |           155.95K |          957.1 |       2751.9 |        679.6 |            1420 |                 9327 |         1014.7 |       26.8 |                 ~0.92 |
| modernRX 0.3.3                       |        4.911M |           155.90K |          952.8 |       2769.4 |        718.0 |            1411 |                 9270 |          926.0 |       27.0 |                 ~0.93 |
| modernRX 0.3.2                       |        4.901M |           154.91K |          954.9 |       2576.2 |        719.7 |            1419 |                 9307 |          934.2 |       25.6 |                 ~0.85 |
| modernRX 0.3.1                       |        4.867M |           156.60K |          958.7 |       2600.1 |        716.6 |            1419 |                 9355 |          911.0 |       26.2 |                 ~0.90 |
| modernRX 0.3.0                       |        4.906M |           156.62K |          957.6 |       2778.6 |        691.0 |            1421 |                 9350 |          889.4 |       26.9 |                 ~0.92 |
| RandomX-1.2.1 (102f8acf)<sup>2</sup> |        3.231M |           103.46K |          881.4 |       2333.3 |        524.7 |            1166 |                 2754 |            --- |       18.6 |                 ~0.84 |
| modernRX 0.2.3                       |        4.872M |           154.30K |          912.7 |       2660.5 |        698.6 |            1394 |                 9356 |          107.8 |       26.2 |                 ~0.87 |
| modernRX 0.2.2                       |        4.893M |           156.04K |          942.2 |       2660.0 |        708.0 |            1415 |                 9419 |           36.9 |       25.8 |                 ~0.83 |
| modernRX 0.2.1                       |        4.903M |           156.11K |          927.9 |       2735.5 |        700.7 |            1436 |                 9458 |           34.3 |       25.9 |                 ~0.89 |
| modernRX 0.2.0                       |        4.902M |           154.70K |          944.4 |       2759.2 |        716.7 |            1419 |                 9409 |           18.3 |       25.8 |                 ~0.86 |
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |        3.231M |           103.46K |          386.7 |       2333.3 |        524.7 |            1166 |                 2754 |            1.9 |       18.6 |                 ~0.84 |
| modernRX 0.1.3 (reference)           |        2.130M |            69.51K |          392.1 |       2761.9 |        724.7 |            1446 |                 8317 |            2.0 |       32.4 |                 ~1.32 |


### Boost clock (up to 4.4Ghz, 95°C temp limit)

|                                      | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Aes1R [MB/s] | Aes4R [MB/s] | AesHash1R [H/s] | Superscalar [Prog/s] | Dataset [MB/s] | Hash [H/s] | Efficiency [H/Watt/s] |
| ------------------------------------ | :-----------: | :---------------: | :------------: | :----------: | :----------: | :-------------: | :------------------: | :------------: | :--------: | :-------------------: |
| RandomX-1.2.1 (102f8acf)             |        4.434M |           133.57K |         1089.3 |      53391.3 |      15955.5 |           30213 |                 5750 |        ~1020.8 |   **5233** |            **~59.46** |
| RandomX-1.2.1 (102f8acf)<sup>7</sup> |        4.434M |           133.57K |         1089.3 |      53391.3 |      15955.5 |           30213 |                 5750 |        ~1020.8 |       3428 |                ~44.34 |
| modernRX 0.7.1                       |        7.563M |           237.46K |         1422.3 |      52413.8 |      15393.3 |           31276 |                13231 |         1310.6 |       3313 |                ~43.59 |
| RandomX-1.2.1 (102f8acf)<sup>6</sup> |        4.434M |           133.57K |         1089.3 |      53391.3 |      15955.5 |           30213 |                 5750 |        ~1020.8 |       3187 |                ~42.49 |
| modernRX 0.7.0                       |        7.506M |           237.90K |	     1443.4 |      54705.8 |      15806.5 |           32010 |                13232 |         1358.8 |       3181 |                ~43.57 |
| RandomX-1.2.1 (102f8acf)<sup>5</sup> |        4.434M |           133.57K |         1089.3 |      53391.3 |      15955.5 |           30213 |                 5750 |        ~1020.8 |      604.8 |                ~16.34 |
| modernRX 0.6.3                       |        7.577M |       **239.14K** |	     1426.3 |      55672.4 |      16505.7 |           32223 |                13244 |         1356.4 |      603.6 |                ~16.44 |
| modernRX 0.6.2                       |    **7.595M** |           239.10K |     **1471.6** |      55782.5 |      16119.8 |       **32389** |            **13275** |         1364.9 |      602.3 |                ~16.19 |
| modernRX 0.6.1                       |        7.579M |           238.78K |         1444.8 |      55314.7 |      15632.4 |           31287 |                12872 |         1356.7 |      449.0 |                ~11.82 |
| modernRX 0.6.0                       |        7.574M |           238.33K |         1456.9 |  **56077.8** |  **16517.6** |           32059 |                13238 |         1365.6 |      397.7 |                ~10.33 |
| RandomX-1.2.1 (102f8acf)<sup>4</sup> |        4.434M |           133.57K |         1089.3 |      53391.3 |      15955.5 |           30213 |                 5750 |        ~1020.8 |       27.4 |                 ~0.61 |
| modernRX 0.5.1                       |        7.552M |           237.47K |         1413.6 |      49992.3 |      16418.3 |           31961 |                13123 |         1353.4 |       92.7 |                 ~2.35 |
| modernRX 0.5.0                       |        7.486M |           236.77K |         1432.3 |      52762.1 |      16332.9 |           31825 |                13095 |         1349.1 |       44.1 |                 ~0.94 |
| RandomX-1.2.1 (102f8acf)<sup>3</sup> |        4.434M |           133.57K |         1089.3 |       3164.2 |        714.7 |            1579 |                 5750 |        ~1020.8 |        --- |                   --- |
| modernRX 0.4.0                       |        7.552M |           237.58K |         1459.0 |       3694.0 |        976.8 |            1943 |                13244 |     **1370.9** |       41.9 |                 ~1.06 |
| modernRX 0.3.12                      |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.11                      |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.10                      |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.9                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.8                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.7                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.6                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.5                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.4                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.3                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.2                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.1                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.3.0                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| RandomX-1.2.1 (102f8acf)<sup>2</sup> |        4.434M |           133.57K |         1089.3 |       3164.2 |        714.7 |            1579 |                 5750 |            --- |        --- |                   --- |
| modernRX 0.2.3                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.2.2                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.2.1                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| modernRX 0.2.0                       |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |        4.434M |           133.57K |          514.0 |       3164.2 |        714.7 |            1579 |                 5750 |            --- |        --- |                   --- |
| modernRX 0.1.3 (reference)           |           --- |               --- |            --- |          --- |          --- |             --- |                  --- |            --- |        --- |                   --- |


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
