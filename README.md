# 文言文 LLVM 編譯器

這是成功大學 114-2「編譯系統」第二次作業的實作。專案的目標是把文言文語法寫成的 `.wy` 程式，經過詞法分析、語法分析與語意處理後產生 LLVM IR，最後編譯成可以直接執行的程式。

一開始看到文言文語法和 LLVM IR 放在一起其實滿衝擊的，因為平常寫 C 或 Python 時，很少需要直接處理 Token、符號表、暫存器與基本區塊。完成這份作業後，我對一個簡單編譯器從原始碼走到執行檔的流程有了比較具體的理解。

## 完成狀態

目前已在課程伺服器通過所有官方測試：

```text
Part 1 (verbose): 64/64
Part 2 (runtime): 41/41
Total:            105/105

All tests passed!
```

除了完成題目要求的功能，我也重新整理了部分內部架構：

- 將 Parser 的暫存狀態集中成 `ParserSession`
- 集中管理 LLVM IR 指令、Label 與 Branch 的文字輸出
- 分離多值資料的型別檢查、深拷貝和預設值建立邏輯
- 讓控制流程模組透過統一介面產生 IR
- 保留作業要求的 CLI、verbose 格式與 Runtime 行為

## 編譯流程

```text
文言文原始碼（.wy）
        │
        ▼
Flex 詞法分析器（compiler.l）
        │
        ▼
Token
        │
        ▼
Bison 語法分析器（compiler.y）
        │
        ▼
語意處理、符號表與型別檢查
        │
        ▼
LLVM IR（.ll）
        │
        ▼
llc 產生 Object File
        │
        ▼
GCC + Wenyan Runtime
        │
        ▼
可執行檔
```

專案提供兩個主要程式：

- `wy`：將 `.wy` 翻譯成 LLVM IR
- `wyc`：執行完整流程，將 `.wy` 編譯成可執行檔

## 支援功能

### 型別與變數

- 32-bit 整數、64-bit 整數與浮點數
- 布林值
- 字串
- 陣列
- 自動型別推導
- 函式
- 單值與多值宣告
- 變數命名、查找與賦值
- 缺少初始值時建立預設值

例如：

```wenyan
吾有一數。曰一。名之曰「甲」。
吾有一數。曰「甲」。書之。
```

輸出：

```text
1
```

### 運算式

- 加、減、乘、除
- 取餘數
- 字串串接
- 整數與浮點數型別提升
- 大於、小於、等於等比較
- 布林 AND、OR
- 鏈式運算

### 控制流程

- `if`
- `else if`
- `else`
- 固定次數的 `for` 迴圈
- 無限 `while` 迴圈
- `break`

這些控制流程會轉換成 LLVM IR 的 Basic Block、Label、條件 Branch 和無條件 Branch。

### 陣列與字串

- 建立陣列
- Push 元素
- 陣列索引
- 字串索引
- 取得陣列或字串長度

### 函式

- 函式定義
- 函式參數
- 回傳值
- 函式呼叫
- 內建函式
- 外部變數 Capture

## 專案結構

```text
.
├── CMakeLists.txt
├── README.md
├── LLVM_IR_CHEATSHEET.md
├── YACC_CHEATSHEET.md
├── cmake/
├── lib/
│   ├── WJCL/
│   └── utf8.c/
├── src/
│   ├── compiler.l
│   ├── compiler.y
│   ├── compiler_util.c
│   ├── compiler_util.h
│   ├── expression.c
│   ├── main.c
│   ├── object.c
│   ├── scope.c
│   ├── value_data.c
│   ├── wyc.c
│   ├── control/
│   │   ├── for.c
│   │   ├── function.c
│   │   ├── if.c
│   │   └── while.c
│   ├── lib/
│   └── wy_rt/
└── test/
    ├── 策問/
    └── 殿試/
```

## 主要模組

### `src/compiler.l`

Flex 詞法分析器，將文言文關鍵字、識別字、數字、字串與運算符號轉成 Token。

中文 UTF-8 的位置計算是這一部分比較麻煩的地方。錯誤訊息除了行號，也需要正確記錄顯示欄位和 Byte Offset。

### `src/compiler.y`

Bison Parser，定義文言文語法並呼叫語意函式。

Parser 使用 `ParserSession` 集中保存：

- 正在處理的多值資料
- 陣列 Push 的目標
- 函式呼叫資訊
- 函式參數型別

這樣比使用多個分散的全域變數更容易追蹤解析狀態。

### `src/scope.c`

管理 Scope 和 Symbol Table。查找變數時會由目前 Scope 往外搜尋，函式使用外層變數時也會在這裡建立 Capture 資訊。

### `src/object.c`

負責編譯器中的 Literal、Symbol、Register、Array 和 Function 表示，也會把物件轉換成 LLVM Operand，例如：

```llvm
42
%reg3
%var.0
@str.1
```

### `src/value_data.c`

文言文可以一次宣告或處理多個值，因此使用 `ValueData` 保存一組資料。這個模組集中處理：

- 宣告數量
- 型別相容性
- Object 深拷貝
- 預設值建立

### `src/expression.c`

負責算術、比較、布林與字串運算，也會處理型別提升。例如 `i32` 與 `double` 運算時，會先產生轉型 IR。

### `src/control/`

- `if.c`：if、else if、else
- `for.c`：固定次數迴圈與 Phi Node
- `while.c`：無限迴圈與 break
- `function.c`：函式定義、呼叫、回傳值與 Capture

### `src/compiler_util.c`

除了錯誤訊息工具，也集中處理 LLVM IR 的一般指令、Label、空行、條件 Branch 與無條件 Branch，讓其他模組不用重複處理 IR 格式。

## 環境需求

| 工具 | 建議版本 |
|---|---|
| CMake | 3.10 以上 |
| Flex | 2.6 以上 |
| Bison | 3.6 以上 |
| GCC | 支援 C11 |
| LLVM / llc | LLVM 14 以上 |

Clone 時要取得 Submodule：

```bash
git clone --recurse-submodules https://github.com/Danielncku/compiler.git
cd compiler
```

若已經 Clone：

```bash
git submodule update --init --recursive
```

## 建置

### Linux

```bash
sudo apt update
sudo apt install cmake flex bison gcc llvm

cmake -B build -S .
cmake --build build
```

### Windows（MSYS2 UCRT64）

```bash
pacman -S bison flex \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-llvm
```

```bash
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
```

若專案路徑包含中文，而 MinGW 顯示找不到相依檔案，可以將專案移到純英文路徑後重新建置。

## 使用方式

產生 LLVM IR：

```bash
./build/wy test/input.wy output.ll
```

顯示完整編譯紀錄：

```bash
./build/wy -v test/input.wy output.ll
```

直接編譯成執行檔：

```bash
./build/wyc test/input.wy output
./output
```

## 測試

Linux / MSYS2：

```bash
./test/test.sh
./test/test.sh -f 00_快速入門
./test/test.sh -n
```

Windows PowerShell：

```powershell
.\test\test.ps1
.\test\test.ps1 -File 00_快速入門
.\test\test.ps1 -NoCompile
```

普通測資位於 `test/策問/`，包含 `00_快速入門` 到 `12_萬化_方術`。綜合測資位於 `test/殿試/`：

- 割圓術
- 曼德博集
- 牛頓求根法

每個案例包含：

- `.wy`：測試程式
- `.verbose`：預期編譯紀錄
- `.expected`：預期執行結果

## 實作心得

這份作業最花時間的地方不是某一個函式，而是很多模組會一起影響結果。一個變數參與運算時，可能依序經過 Parser、Symbol Table、Object 載入、型別判斷、型別轉換，最後才產生運算指令。

控制流程也不能只把 Label 印出來就結束。每個 Basic Block 都要有合法的 Terminator，迴圈還要注意回到 Header 的路徑，以及 `break` 要跳到正確的 Exit Label。

另一個容易踩坑的地方是記憶體所有權。Parser 的語意值、Symbol Table 借出的指標、Register 複製，以及字串和數字的深拷貝，如果沒有先約定清楚，很容易發生重複釋放或懸空指標。

最後能讓課程伺服器上的 Verbose 和 Runtime 全部通過，算是把原本看起來很零碎的 Compiler 知識串起來了。目前 Bison 仍會顯示部分 Conflict 警告，這是未來還能繼續整理的方向，但不影響官方測資的編譯與執行。

## 參考資料

- [NCKU Compiler HW2 作業說明](https://hackmd.io/@WavJaby/NCKU_1142_COMPILER_HW2)
- [LLVM Language Reference](https://llvm.org/docs/LangRef.html)
- [GNU Bison Manual](https://www.gnu.org/software/bison/manual/)
- [Flex Manual](https://westes.github.io/flex/manual/)

## 說明

本專案用於課程學習與編譯器實作練習。`lib/WJCL`、`lib/utf8.c` 與作業框架的著作權及授權依原專案規定。
