# Quick Start

## 1. Compile source code

In das-project/Homework_1/c_src/

```bash
make
```

## 2. Using the program with multitask.sh

You can use the compiled program directly.

But it is best to use the multitask.sh script

```bash
./multitask.sh -t 10
```

This starts 10 instances of the program (if -t is not provided it starts 4).
The resulting data is placed in a file named historical_data.csv

If you want to set which assets the program collects, write the asset
in the .allowed_assets file **(each asset must be written in a new line)**

## 3. Using the program alone

After compiling

```bash
./get_data.out
```

For more info

```bash
./get_data.out --help
```
