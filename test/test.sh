#!/bin/bash
# Tests for wenyan-llvm.
# Part 1: verbose compiler output  (wy -v)
# Part 2: runtime execution output (wyc + run)
#
# Usage: ./test/test.sh [options]
#   -f <name>   filter by filename substring
#   -s          stop on first failure
#   -n          skip rebuild
#   -e          English mode (questions_en + questions_advanced_en, VERBOSE_EN=ON)
#   -i          interactive diff (with pager)
#   -h          show this help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

FILE_FILTER=""
STOP_ON_FIRST_ERROR=false
NO_COMPILE=false
INTERACTIVE=false
ENGLISH_MODE=false
BUILD_DIR="$ROOT/build"

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--file)       FILE_FILTER="$2"; shift 2 ;;
        -s|--stop)       STOP_ON_FIRST_ERROR=true; shift ;;
        -n|--no-compile) NO_COMPILE=true; shift ;;
        -e|--english)    ENGLISH_MODE=true; shift ;;
        -b|--build-dir)  BUILD_DIR="$2"; shift 2 ;;
        -i|--interactive) INTERACTIVE=true; shift ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \?//'; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

CYAN='\033[0;36m'; RED='\033[0;31m'; GREEN='\033[0;32m'
YELLOW='\033[0;33m'; BOLD='\033[1m'; NC='\033[0m'

# ── build ─────────────────────────────────────────────────────────────────────
if [[ "$NO_COMPILE" == false ]]; then
    echo -e "${CYAN}Compiling...${NC}"
    CMAKE_EXTRA_ARGS=""
    [[ "$ENGLISH_MODE" == true ]] && CMAKE_EXTRA_ARGS="-DVERBOSE_EN=ON"
    if [[ ! -d "$BUILD_DIR" ]]; then
        cmake -B "$BUILD_DIR" -S "$ROOT" -DCMAKE_BUILD_TYPE=Release $CMAKE_EXTRA_ARGS \
            || { echo -e "${RED}CMake configure failed.${NC}"; exit 1; }
    fi
    cmake --build "$BUILD_DIR" || { echo -e "${RED}Build failed.${NC}"; exit 1; }
fi

WY="$BUILD_DIR/wy"
WYC="$BUILD_DIR/wyc"
[[ -x "$WY"  ]] || { echo -e "${RED}wy not found${NC}";  exit 1; }
[[ -x "$WYC" ]] || { echo -e "${RED}wyc not found${NC}"; exit 1; }

TMPDIR_WORK="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_WORK"' EXIT

normalize() { sed '1s/^\xef\xbb\xbf//' | tr -d '\r' \
    | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}' | sed '/./,$!d'; }

show_diff() {
    printf '%s\n' "$1" > "$TMPDIR_WORK/expected.txt"
    printf '%s\n' "$2" > "$TMPDIR_WORK/actual.txt"
    local OPTS="-c core.autocrlf=false diff --ignore-cr-at-eol --no-index --color=always"
    if [[ "$INTERACTIVE" == true ]]; then
        git --no-pager $OPTS "$TMPDIR_WORK/expected.txt" "$TMPDIR_WORK/actual.txt" 2>/dev/null | less -R +Gg
    else
        git --no-pager $OPTS "$TMPDIR_WORK/expected.txt" "$TMPDIR_WORK/actual.txt" || true
    fi
}

# ── scoring per test ──────────────────────────────────────────────────────────
# 00: unscored (practice)
# W1  01:         12v +  8e = 20
# W2  02–05:  4×(  3v +  2e) = 20
# W3  06–11:  6×(  3v +  2e) = 30
# W4  12:          4v +  2e =  6 ; 殿試×3: 5v+3e=8 each → 24   W4 total=30
# Grand total: 61v + 39e = 100
declare -A TEST_V_PTS=(
    ["00_快速入門"]=3   ["00_quick_start"]=3
    ["01_開物_定名"]=12 ["01_declare_naming"]=12
    ["02_流轉_易值"]=3  ["02_assign_update"]=3
    ["03_留墨_書法"]=3  ["03_print_string"]=3
    ["04_九章_算術"]=3  ["04_arithmetic"]=3
    ["05_山村_算術"]=3  ["05_village_arithmetic"]=3
    ["06_尋隱_決策"]=3  ["06_decision"]=3
    ["07_酒宴_循環"]=3  ["07_loop"]=3
    ["08_口訣_乘算"]=3  ["08_multiplication_table"]=3
    ["09_百雞_算術"]=3  ["09_hundred_chickens"]=3
    ["10_客上_倒敘"]=3  ["10_reverse_string"]=3
    ["11_布陣_行列"]=3  ["11_array"]=3
    ["12_萬化_方術"]=4  ["12_function"]=4
    ["割圓術"]=5  ["circle_division"]=5
    ["曼德博集"]=5  ["mandelbrot"]=5
    ["牛頓求根法"]=5  ["newton_root"]=5
)
declare -A TEST_E_PTS=(
    ["00_快速入門"]=2   ["00_quick_start"]=2
    ["01_開物_定名"]=8  ["01_declare_naming"]=8
    ["02_流轉_易值"]=2  ["02_assign_update"]=2
    ["03_留墨_書法"]=2  ["03_print_string"]=2
    ["04_九章_算術"]=2  ["04_arithmetic"]=2
    ["05_山村_算術"]=2  ["05_village_arithmetic"]=2
    ["06_尋隱_決策"]=2  ["06_decision"]=2
    ["07_酒宴_循環"]=2  ["07_loop"]=2
    ["08_口訣_乘算"]=2  ["08_multiplication_table"]=2
    ["09_百雞_算術"]=2  ["09_hundred_chickens"]=2
    ["10_客上_倒敘"]=2  ["10_reverse_string"]=2
    ["11_布陣_行列"]=2  ["11_array"]=2
    ["12_萬化_方術"]=2  ["12_function"]=2
    ["割圓術"]=3  ["circle_division"]=3
    ["曼德博集"]=3  ["mandelbrot"]=3
    ["牛頓求根法"]=3  ["newton_root"]=3
)

if [[ "$ENGLISH_MODE" == true ]]; then
    TEST_DIRS=("$SCRIPT_DIR/questions_en" "$SCRIPT_DIR/questions_advanced_en")
else
    TEST_DIRS=("$SCRIPT_DIR/策問" "$SCRIPT_DIR/殿試")
fi

collect_wy_files() {
    local dir="$1"
    [[ -d "$dir" ]] || return
    find "$dir" -maxdepth 1 -name "*.wy" -print0 | sort -z
}

ALL_PASSED=true
PART1_SCORE=0; PART1_MAX=0
PART2_SCORE=0; PART2_MAX=0

# ── Part 1: Verbose ───────────────────────────────────────────────────────────
echo -e "\n${BOLD}${CYAN}══ Part 1: Verbose Output ══${NC}"

for dir in "${TEST_DIRS[@]}"; do
    [[ -d "$dir" ]] || continue

    while IFS= read -r -d '' wy_file; do
        filename="$(basename "$wy_file")"
        [[ -n "$FILE_FILTER" && "$filename" != *"$FILE_FILTER"* ]] && continue

        base="${wy_file%.wy}"
        stem="$(basename "$base")"
        verbose_file="$base.verbose"
        pts="${TEST_V_PTS[$stem]:-0}"
        PART1_MAX=$(( PART1_MAX + pts ))

        if [[ ! -f "$verbose_file" ]]; then
            echo -e "  ${YELLOW}SKIP${NC}  $filename  (no reference)"
            continue
        fi

        actual=$("$WY" -v "$wy_file" "$TMPDIR_WORK/_ir.ll" 2>&1 | normalize) || true
        expected=$(normalize < "$verbose_file")

        if [[ "$actual" == "$expected" ]]; then
            PART1_SCORE=$(( PART1_SCORE + pts ))
            echo -e "  ${GREEN}PASS${NC}  $filename  (+${pts})"
        else
            ALL_PASSED=false
            echo -e "  ${RED}FAIL${NC}  $filename"
            if [[ "$INTERACTIVE" == true ]]; then
                show_diff "$expected" "$actual"
            else
                echo "  ── diff ──────────────────────────────────"
                show_diff "$expected" "$actual" | sed 's/^/  /'
                echo "  ──────────────────────────────────────────"
            fi
            [[ "$STOP_ON_FIRST_ERROR" == true ]] && break 2
        fi
    done < <(collect_wy_files "$dir")
done

echo -e "${CYAN}  Subtotal: ${PART1_SCORE}/${PART1_MAX}${NC}"

# ── Part 2: Runtime Output ───────────────────────────────────────────────────
echo -e "\n${BOLD}${CYAN}══ Part 2: Runtime Output ══${NC}"

for dir in "${TEST_DIRS[@]}"; do
    [[ -d "$dir" ]] || continue

    while IFS= read -r -d '' wy_file; do
        filename="$(basename "$wy_file")"
        [[ -n "$FILE_FILTER" && "$filename" != *"$FILE_FILTER"* ]] && continue

        base="${wy_file%.wy}"
        stem="$(basename "$base")"
        expected_file="$base.expected"
        pts="${TEST_E_PTS[$stem]:-0}"
        PART2_MAX=$(( PART2_MAX + pts ))

        if [[ ! -f "$expected_file" ]]; then
            echo -e "  ${YELLOW}SKIP${NC}  $filename  (no reference)"
            continue
        fi

        exe="$TMPDIR_WORK/$(basename "$base")"
        compile_rc=0
        compile_err=$("$WYC" "$wy_file" "$exe" 2>&1) || compile_rc=$?
        if [[ $compile_rc -ne 0 ]]; then
            ALL_PASSED=false
            echo -e "  ${RED}FAIL${NC}  $filename  (compile error)"
            echo "$compile_err" | sed 's/^/    /'
            [[ "$STOP_ON_FIRST_ERROR" == true ]] && break 2
            continue
        fi

        actual=$("$exe" 2>&1 | normalize) || true
        expected=$(normalize < "$expected_file")

        if [[ "$actual" == "$expected" ]]; then
            PART2_SCORE=$(( PART2_SCORE + pts ))
            echo -e "  ${GREEN}PASS${NC}  $filename  (+${pts})"
        else
            ALL_PASSED=false
            echo -e "  ${RED}FAIL${NC}  $filename"
            if [[ "$INTERACTIVE" == true ]]; then
                show_diff "$expected" "$actual"
            else
                echo "  ── diff ──────────────────────────────────"
                show_diff "$expected" "$actual" | sed 's/^/  /'
                echo "  ──────────────────────────────────────────"
            fi
            [[ "$STOP_ON_FIRST_ERROR" == true ]] && break 2
        fi
    done < <(collect_wy_files "$dir")
done

echo -e "${CYAN}  Subtotal: ${PART2_SCORE}/${PART2_MAX}${NC}"

# ── Summary ───────────────────────────────────────────────────────────────────
TOTAL=$(( PART1_SCORE + PART2_SCORE ))
MAX=$(( PART1_MAX + PART2_MAX ))

echo ""
echo -e "${BOLD}${CYAN}══ Score ══${NC}"
echo -e "  Part 1 (verbose):  ${PART1_SCORE}/${PART1_MAX}"
echo -e "  Part 2 (runtime):  ${PART2_SCORE}/${PART2_MAX}"
echo -e "  ${BOLD}Total:             ${TOTAL}/${MAX}${NC}"

if [[ "$ALL_PASSED" == true ]]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed.${NC}"
    exit 1
fi
