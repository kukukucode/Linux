# Project scope

このリポジトリには、Linuxの内部機構について実装・検証した成果を残します。

各テーマには、可能な範囲で次の情報を含めます。

- 実行可能なCプログラム
- 正常系と異常系のtest
- resource ownershipの設計
- `strace`、GDB、`/proc`から得た観察結果
- 実装時に見つかった問題と修正内容

## Memory and ownership

- stackとheapの配置
- pointerとresource lifetime
- structure alignmentとmemory layout
- `mmap()`とvirtual memory

## Processes and descriptors

- processとsystem call
- file descriptor
- `fork()`、`exec()`、`waitpid()`
- `pipe()`、`dup2()`、redirection
- signal

## Mini shell

pipelineとredirectionを実行できるshellを実装します。

```sh
mysh> ls | grep foo > result.txt
```

## TCP echo server

- `socket()`、`bind()`、`listen()`、`accept()`
- partial read/write
- signalを使ったshutdown
- threadとresource ownership

## TCP reverse proxy

- clientとupstreamの双方向転送
- non-blocking I/Oとbackpressure
- timeoutとgraceful shutdown
- 共有状態とmetrics
