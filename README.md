# Linux

Linux の内部機構を C で実装し、コード・テスト・調査記録として残すリポジトリです。

主な成果物:

```text
mini shell
    ↓
TCP echo server
    ↓
TCP reverse proxy
```

実装と調査で扱うテーマは、stack / heap、pointer、ownership、memory layout、process、
file descriptor、system call、socket、signal、thread、`mmap`、`fork / exec`、
`strace`、`gdb`、`/proc` です。

## Repository status

C17 のbuild、test、CIを実行できる最小構成です。以降のcommitで、検証可能な
小さな実装と、その挙動を確認した記録を追加します。

## 必要な環境

- Linux（Windows では WSL2 の Ubuntu を推奨）
- GCC または Clang
- GNU Make
- GDB
- strace

Ubuntu での準備例:

```sh
sudo apt update
sudo apt install build-essential gdb strace
```

## ビルド

```sh
make
./build/hello
make test
```

## Project scope

実装対象と記録方針は [docs/project-scope.md](docs/project-scope.md) にまとめています。
