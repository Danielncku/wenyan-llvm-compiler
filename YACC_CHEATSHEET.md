# YACC / Bison Cheatsheet — 文言文編譯器作業專用

補充 Bison 進階用法，聚焦在這份作業實際會碰到的情境。實作 `compiler.y` 遇到問題時查。

相關文件：[作業說明](https://hackmd.io/@WavJaby/NCKU_1142_COMPILER_HW2)　[README](README.md)　[LLVM IR Cheatsheet](LLVM_IR_CHEATSHEET.md)

---

### 本專案的 `$N`

上方的 `$N` 範例使用單一型別。本專案的 `%union` 包含多種欄位，需加上型別標注，否則 Bison 會取錯欄位：

```yacc
ValueLiteralStmt
    : NUMBER_LIT { $$ = object_createNumber(&$<n_var>1); }
    /*            ^^                              ^^
              回傳給呼叫者               手動指定取 n_var 欄位  */
```

**何時需要手動標注：**
- Token 已用 `%token <欄位>` 宣告 → 不需標注
- 非終端符號已用 `%type <欄位>` 宣告 → 不需標注
- 其他情況 → **必須**手動標注 `$<欄位>N`

```yacc
FunctionArgListStmt
    : SAID IDENT { func_defineAddParam($<var_type>0, $<s_var>2); }
    /*                                 ^^^^^^^^^^^ 手動指定   */
```

**`%union` 欄位速查：**

| 用途 | 欄位名 | C 型別 |
|------|--------|--------|
| 變數 / 函式型別 | `var_type` | `ObjectType` |
| 布林字面值 | `b_var` | `bool` |
| 數字字面值 | `n_var` | `ScientificNotation` |
| 字串 / 識別字 | `s_var` | `char*` |
| 語意值（運算式結果） | `obj_val` | `Object` |
| 多值宣告容器 | `val_data` | `ValueData` |
| 函式呼叫資訊 | `func_call` | `FuncCallInfo*` |
| 算符方向（介詞） | `exp_left` | `bool` |
| 算符種類 | `exp_op` | `ExpOp` |

---

### `$0` 和 `$-1` — 讀取父規則左邊的符號

在一條產生式的動作裡，`$0` 指的是**父規則中緊接在本非終端符號左邊的符號**的語意值，`$-1` 再往左一格。

```
OperationStmt
    : CreateValueDataListStmt  NAME_IT  VariableDefineStmt
      ─────────────────────    ───────  ──────────────────
              $-1                $0        （當前規則）
```

當 Bison reduce `VariableDefineStmt` 時，其動作內可存取：
- `$<val_data>-1` → `CreateValueDataListStmt` 的值（已宣告的值列表）
- `$<var_type>0` → 左邊符號的值（本例為關鍵字，通常不用）

**若父規則在 `VariableDefineStmt` 前有 mid-rule action，索引會往右移一格：**

```
OperationStmt
    : CreateValueDataListStmt  LitOrVarStmt  { mid-rule }  VariableDefineStmt
      ─────────────────────    ────────────  ───────────   ──────────────────
               $-2                 $-1           $0           （當前規則）
```

此時 `$<val_data>0` 取得 mid-rule action 的 `$$`（通常由 mid-rule 將 val_data 存入）。具體索引取決於你自己設計的語法結構，實作時數清楚當前規則左邊有幾個符號再決定用哪個。

```
FunctionArgsStmt
    : TO_PERFORM_FUNC REQUIRE_ARGS NUMBER_LIT  VAR_TYPE  FunctionArgListStmt
                                               ────────  ──────────────────
                                                  $0        （當前規則）
```

`FunctionArgListStmt` 動作內的 `$<var_type>0` 取得 `VAR_TYPE`（參數型別）。

**本專案常見位置：**

| 出現位置 | 讀取的是什麼 | 索引 |
|---------|------------|------|
| `VariableDefineStmt`（無 mid-rule） | 父規則的 `CreateValueDataListStmt` | `$<val_data>-1` |
| `VariableDefineStmt`（有 mid-rule 在左邊） | mid-rule 存入的 val_data | `$<val_data>0` |
| `FunctionArgListStmt` | 父規則的 `VAR_TYPE` | `$<var_type>0` |

其餘 `$0` 使用情境（印出、多值宣告、函式呼叫引數等）皆在 `OperationStmt` 的子規則中，實作時參照上方模式即可。

**注意：** Bison 不做型別或邊界檢查，標注錯誤不報錯，只會拿到垃圾值。

---

### Mid-Rule Action — 在規則中間執行動作

Bison 允許在右手邊符號之間插入動作 `{ ... }`，該動作在 value stack 上佔一個位置，後續 `$N` 編號順移：

```yacc
SomeRule
    : A B { $$ = something; } C
    /*       ^^^^^^^^^^^^^^
              mid-rule action，佔 stack 第 3 位
              A=$1, B=$2, mid-action=$3, C=$4   */
```

**本專案實例：`PUSH` 語法（你將在 OperationStmt 實作此模式）**

```yacc
OperationStmt
    : PUSH VariableStmt { $<obj_val>$ = $<obj_val>2; } PushItemList
    /*      ^^^^^^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^^
            $2（陣列）   mid-rule action，存入 stack 讓子規則透過 $0 拿到  */

PushItemList
    : EXP_PREPOSITION LitOrVarStmt
        { code_arrayPush(&$<obj_val>0, &$<obj_val>2, &@2); }
    /*                    ^^^^^^^^^^
                $0 = 上方 mid-rule action 存入的陣列 Object  */
    | PushItemList EXP_PREPOSITION LitOrVarStmt
        { code_arrayPush(&$<obj_val>0, &$<obj_val>3, &@3); }
;
```

Stack 狀態示意：

```
... | PUSH | VariableStmt | mid-action | (PushItemList 的符號)
                                 ↑
                           $0（陣列 Object）
```

---

### 本專案的優先序宣告

`compiler.y` 已提供兩個虛擬 token 用於 `%prec`：

```yacc
%nonassoc LOWER_THAN_EXPR   /* 虛擬 token，賦予「比 EXPR 低」的優先序 */
%nonassoc RETURN
```

虛擬 token 不會出現在輸入中，專門搭配 `%prec` 讓特定規則借用其優先序：

```yacc
ReturnStmt
    : RETURN ExpressionChainStmt %prec LOWER_THAN_EXPR { ... }
    /* 空產生式或末端無 token 時，用 %prec 指定解衝突依據 */
```

**實作 ValueStmt、ExpressionStmt 時** 遇到 shift/reduce 衝突，用 `bison -v compiler.y` 查看 `compiler.output`，再視情況補 `%left`/`%nonassoc`。`compiler.y` 已留一行 `%left INDEX` 作為格式參考。

**預設動作：** 若規則沒有寫 `{ ... }`，Bison 預設執行 `{ $$ = $1; }`。

---

### 位置資訊 `@N`

```yacc
ExpressionStmt
    : EXP_MATH_OP ValueStmt EXP_PREPOSITION ValueStmt
        { $$ = code_expression(..., &@2, &@4); }
        /*                         ^^   ^^
                第 2、第 4 個符號的來源位置（行號、欄號）
                型別為 YYLTYPE，用於錯誤訊息定位          */
```

---

### `YYABORT` 與錯誤處理

```yacc
VariableStmt
    : IDENT {
        if (($$ = scope_findSymbol($<s_var>1)).type == OBJECT_TYPE_UNDEFINED)
            YYABORT;   /* 語意錯誤，立即終止解析 */
    }
;
```

- `YYABORT`：立即停止整個解析，回傳錯誤。
- 慣例：C 函式回傳 `true` 表示失敗、`false` 表示成功。
- `yyerrorf("訊息")` 只印出錯誤，**不會**自動停止，需自己加 `YYABORT`。

---

### Shift/Reduce 衝突

#### 什麼是 Shift/Reduce 衝突

Parser 讀到一個 token 時，同時有兩個合法動作：

- **Shift**：把這個 token 壓進 stack，繼續往下讀
- **Reduce**：用目前 stack 頂端的符號序列，套用某條規則歸約

兩個都合法但只能選一個，就是衝突。

**經典例子 — dangling else：**

```yacc
IfStmt
    : IF Cond THEN Stmt
    | IF Cond THEN Stmt ELSE Stmt
```

讀到 `ELSE` 時：
- Shift → `ELSE` 歸屬於最近的 `IF`（通常是正確行為）
- Reduce → 先把前面的 `IF Cond THEN Stmt` 歸約，`ELSE` 歸屬於外層 `IF`

Bison 預設選 **Shift**（通常是你要的），但會印出警告。

#### 本專案中常見的 S/R 衝突位置

| 規則 | 容易衝突的地方 | 原因 |
|------|--------------|------|
| `ValueStmt` | 看到 `INDEX`（之） | 不確定是索引還是下一條語句的開頭 |
| `ExpressionStmt` | 鏈式運算的結尾 | 下一個 token 是繼續鏈還是新語句 |
| `LitOrVarStmt` | `SAID` 後接 `IDENT` | 可能是變數引用或函式名稱 |

#### 如何讀 compiler.output

Build 完後，`build/generated/compiler.output` 已自動生成（`CMakeLists.txt` 設定了 `-v -Wcounterexamples`，不需要自己加 flag）。

找衝突的方式：

```
grep -n "conflict\|衝突" build/generated/compiler.output
```

衝突段落格式：

```
State 42 conflicts: 1 shift/reduce
...
State 42
  ExpressionStmt -> ValueStmt .           (rule 17)
  ExpressionStmt -> ValueStmt . EXP_MATH_OP ValueStmt  (rule 18)

  EXP_MATH_OP   shift, and go to state 55   ← Shift 動作
  EXP_MATH_OP   reduce using rule 17        ← Reduce 動作（衝突）
```

`-Wcounterexamples` 會額外印出造成衝突的具體 token 序列範例，方便理解。

#### 解決方法

| 情況 | 解法 |
|------|------|
| 運算子優先序問題 | 在宣告區加 `%left`/`%right`/`%nonassoc TOKEN` |
| 特定規則要強制 Reduce | 在規則結尾加 `%prec LOWER_THAN_XXX` |
| Bison 預設 Shift 就是你要的 | 用 `%expect N` 聲明預期 N 個 S/R 衝突，消除警告 |
| 語法設計本身有歧義 | 重構規則，拆出更明確的非終端符號 |

```yacc
%left  EXP_MATH_OP      /* 左結合，同優先序 */
%right EXP_ASSIGN       /* 右結合 */
%nonassoc EXP_COMPARE   /* 不可連鎖：a < b < c 是錯誤 */
```

---

### Reduce/Reduce 衝突

#### 什麼是 Reduce/Reduce 衝突

同一個 stack 狀態可以套用**兩條不同規則**歸約，Bison 不知道選哪條。

```yacc
A : X Y ;
B : X Y ;   /* 和 A 右手邊完全相同 */
```

比 Shift/Reduce 衝突更嚴重：Bison 選第一條（宣告順序），**靜默吃掉錯誤**，語義通常是錯的。

#### 常見原因與修法

| 原因 | 範例 | 修法 |
|------|------|------|
| 兩條規則右手邊相同 | `LitStmt : NUMBER_LIT` 和 `ValStmt : NUMBER_LIT` | 合併成一條，或加前置 token 區分 |
| 空產生式歧義 | 多個 optional 規則都能歸約為空 | 重構，讓每條規則有唯一的 token 起點 |
| `%type` 宣告了相同規則給不同非終端 | — | 檢查是否有重複的 token 序列路徑 |

診斷：在 `compiler.output` 裡搜尋 `reduce/reduce`：

```
State 7 conflicts: 1 reduce/reduce
```

---

### Bison 編譯後 Parser 卡住（無窮迴圈 / 不動）

#### 原因 1：空產生式互相引用

```yacc
A : B ;
B : A ;   /* A → B → A → B → ... */
```

或者更隱晦的間接環：

```yacc
OperationStmt :            /* 空 */
CreateValueDataListStmt :  /* 空 */
OperationStmt : CreateValueDataListStmt OperationStmt
```

Parser 會不斷嘗試 reduce 空規則，陷入無窮迴圈，**程式看起來卡死**。

**診斷：** 用 `-v` 生成的 `compiler.output`，搜尋你的規則，看有沒有 `ε`（空產生式）形成環路。

**修法：** 確保至少有一條非空的基底規則；空產生式只放在真正可選的地方。

#### 原因 2：Flex 規則沒有回傳 token

```c
"某文言字串" { /* 忘記 return TOKEN_NAME; */ }
```

Lexer 吃掉這個字串但什麼都不回傳，Parser 一直等下一個 token，**看起來卡死**。

**診斷：** 在 `compiler.l` 開啟 lexer verbose（`-l` flag）或加 `fprintf(stderr, ...)` 確認每條規則有沒有觸發並回傳。

**修法：** 每條 token 規則末尾加 `return TOKEN_NAME;`。

#### 原因 3：語意動作裡有無窮迴圈

語意動作 `{ ... }` 裡呼叫了某個函式，而那個函式內部有無窮迴圈或 deadlock。

**診斷：** 用 `gdb` attach 或 `Ctrl+C` 後看 stack trace；或在每個語意動作加 `fprintf(stderr, "rule X\n")` 確認執行到哪裡停住。

#### 症狀速查

| 症狀 | 最可能原因 |
|------|----------|
| 程式執行後完全沒輸出，也不結束 | 空產生式迴圈 或 Flex 沒有 return |
| 只有部分測試卡住 | 特定 token 的 Flex 規則缺 return |
| 加了 `fprintf` 後發現某個語意動作不斷執行 | 空產生式迴圈 |
| `gdb` stack trace 停在 `yyparse` 內部 | Flex 沒有 return（Parser 等 token） |

---

### 常見錯誤

| 症狀 | 原因 | 對策 |
|------|------|------|
| Shift/Reduce 衝突警告 | 優先序未設好或產生式歧義 | 查 `build/generated/compiler.output`，補 `%left`/`%prec` |
| Reduce/Reduce 衝突警告 | 兩條規則右手邊相同 | 重構規則，加前置 token 區分 |
| 值看起來是亂數或 null | `$<type>N` 欄位標錯 | 對照 `%union` 確認欄位名 |
| 語意值莫名錯誤但無報錯 | 忘記賦值 `$$` | 確認每條有回傳值的規則都設定了 `$$` |
| `$0`/`$-1` 拿到錯誤值 | 非終端符號出現在多條父規則中 | 確認該非終端符號只有一種父規則呼叫情境 |
| `free` 後存取 / double free | `STR_LIT`/`IDENT` 的 `$<s_var>N` 生命週期 | 傳出去的字串不要 free；消費後未傳出須呼叫 `object_free` |
| Parser 卡死不結束 | Flex 規則缺 `return` 或空產生式成環 | 加 lexer verbose 確認；查 `compiler.output` 找空產生式環 |
