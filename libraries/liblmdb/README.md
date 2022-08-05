# Benchmark Instruction

This is an instruction to benchmark LMDB for experiments in AirIndex: Versatile Index Tuning Through Data and Storage.

Please follow [dataset](https://github.com/illinoisdata/airindex-public/blob/main/dataset_setup.md) and [query key set](https://github.com/illinoisdata/airindex-public/blob/main/keyset_setup.md) instructions to setup the benchmarking environment. These are examples of environment [reset scripts](https://github.com/illinoisdata/airindex-public/blob/main/reload_examples.md). The following assumes that the datasets are under `/path/to/data/`, key sets are under `/path/to/keyset/` and output files are under `/path/to/output`

## Build

To build the binaries

```
make install
```

To build indexes for all datasets

```
bash build.sh file:///path/to/output file:///path/to/data
```

## Benchmark (5.2)

Benchmark over 40 key set of 1M keys

```
bash benchmark.sh file:///path/to/output file:///path/to/keyset ~/reload_local.sh
```

The measurements will be recorded in `/path/to/output/out` folder.

## Build Scalability (5.6)

To measure the build time, run the build script.

```
bash scale.sh file:///path/to/output file:///path/to/data
```

