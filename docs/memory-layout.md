# Memory Layout

Cプログラム内のデータが、Linuxプロセスの仮想メモリ上でどこに配置されるかを観察した。

## Experiment

`src/memory_layout.c` で以下のアドレスを表示した。

- initialized global variable
- uninitialized global variable
- static variable
- string literal
- `malloc()` で確保した領域
- local variable

実行:

```sh
./build/memory_layout

別ターミナルから実行中プロセスのmemory mapを確認した。

cat /proc/<PID>/maps

ELF内のsectionも確認した。

readelf -S build/memory_layout | grep -E '\.(text|rodata|data|bss)'
nm -n build/memory_layout | grep ' main$'
Observations
.text

main() は .text sectionに配置されていた。

.text は実行可能な r-xp mappingにロードされる。

.rodata

文字列リテラルはread-onlyのmapping内に存在していた。

r--p
.data

初期値を持つglobal/static変数は .data に配置された。

.bss

未初期化global変数は .bss に配置された。

.bss はELF上では NOBITS であり、ゼロ初期化領域そのものをファイルに保存する必要がない。

Heap

malloc() で確保したアドレスは /proc/<PID>/maps の [heap] 範囲内に存在した。

Stack

local variableのアドレスは [stack] 範囲内に存在した。

Summary

今回の観察から、概念的には以下のような配置になることを確認した。

low address

.text
.rodata
.data
.bss

heap
   ...

   ...
stack

high address

実際の絶対アドレスはASLRによって実行ごとに変化するため、
固定されたアドレスではなくmappingとの対応を見る必要がある。