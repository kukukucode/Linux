# Development Environment

## Purpose

Linux内部機構をCで実装・観察するための開発環境を用意する。

## Tools

- Ubuntu
- GCC
- GNU Make
- GDB
- strace
- /proc

## Verification

```sh
make
./build/hello
make test

gcc --version
make --version
gdb --version
strace --version

cat /proc/version
cat /proc/$$/status
ls -l /proc/$$/fd