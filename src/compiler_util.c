#include "compiler_util.h"

#include <stdarg.h>
#include <string.h>
#include <utf8.c/utf8.h>

#include "lib/code_gen.h"
#include "lib/tab_width.h"

#define LINE_PREFIX "%6d |%s"
#define LINE_PREFIX_EMPTY "       |"

extern int TAB_WIDTH;

IRTextBuilder irTextBuilder(ByteBuffer* output) {
    return (IRTextBuilder){.output = output};
}

void irEmit(IRTextBuilder* builder, const char* format, ...) {
    char line[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    buffPrintln(builder->output, "%s", line);
}

void irEmitBlank(IRTextBuilder* builder) {
    buffPrintln(builder->output, "");
}

void irEmitLabel(IRTextBuilder* builder, const char* format, ...) {
    char label[512];
    va_list args;
    va_start(args, format);
    vsnprintf(label, sizeof(label), format, args);
    va_end(args);
    buffPrintlnS(builder->output, "%s:", label);
}

void irEmitBranch(IRTextBuilder* builder, const char* format, ...) {
    char label[512];
    va_list args;
    va_start(args, format);
    vsnprintf(label, sizeof(label), format, args);
    va_end(args);
    irEmit(builder, "br label %%%s", label);
}

void irEmitConditionalBranch(IRTextBuilder* builder, const char* condition,
                             const char* trueLabel, const char* falseLabel) {
    irEmit(builder, "br i1 %s, label %%%s, label %%%s",
           condition, trueLabel, falseLabel);
}

bool isFullWidth(const uint32_t c) {
    return
        c == 0x2329u || c == 0x232Au || c == 0x23F0u || c == 0x23F3u || c == 0x267Fu || c == 0x2693u || c == 0x26A1u ||
        c == 0x26CEu || c == 0x26D4u || c == 0x26EAu || c == 0x26F5u || c == 0x26FAu || c == 0x26FDu || c == 0x2705u ||
        c == 0x2728u || c == 0x274Cu || c == 0x274Eu || c == 0x2757u || c == 0x27B0u || c == 0x27BFu || c == 0x2B50u ||
        c == 0x2B55u || c == 0x3000u || c == 0x3004u || c == 0x3005u || c == 0x3006u || c == 0x3007u || c == 0x3008u ||
        c == 0x3009u || c == 0x300Au || c == 0x300Bu || c == 0x300Cu || c == 0x300Du || c == 0x300Eu || c == 0x300Fu ||
        c == 0x3010u || c == 0x3011u || c == 0x3014u || c == 0x3015u || c == 0x3016u || c == 0x3017u || c == 0x3018u ||
        c == 0x3019u || c == 0x301Au || c == 0x301Bu || c == 0x301Cu || c == 0x301Du || c == 0x3020u || c == 0x3030u ||
        c == 0x303Bu || c == 0x303Cu || c == 0x303Du || c == 0x303Eu || c == 0x309Fu || c == 0x30A0u || c == 0x30FBu ||
        c == 0x30FFu || c == 0x3250u || c == 0xA015u || c == 0xFE17u || c == 0xFE18u || c == 0xFE19u || c == 0xFE30u ||
        c == 0xFE35u || c == 0xFE36u || c == 0xFE37u || c == 0xFE38u || c == 0xFE39u || c == 0xFE3Au || c == 0xFE3Bu ||
        c == 0xFE3Cu || c == 0xFE3Du || c == 0xFE3Eu || c == 0xFE3Fu || c == 0xFE40u || c == 0xFE41u || c == 0xFE42u ||
        c == 0xFE43u || c == 0xFE44u || c == 0xFE47u || c == 0xFE48u || c == 0xFE58u || c == 0xFE59u || c == 0xFE5Au ||
        c == 0xFE5Bu || c == 0xFE5Cu || c == 0xFE5Du || c == 0xFE5Eu || c == 0xFE62u || c == 0xFE63u || c == 0xFE68u ||
        c == 0xFE69u || c == 0xFF04u || c == 0xFF08u || c == 0xFF09u || c == 0xFF0Au || c == 0xFF0Bu || c == 0xFF0Cu ||
        c == 0xFF0Du || c == 0xFF3Bu || c == 0xFF3Cu || c == 0xFF3Du || c == 0xFF3Eu || c == 0xFF3Fu || c == 0xFF40u ||
        c == 0xFF5Bu || c == 0xFF5Cu || c == 0xFF5Du || c == 0xFF5Eu || c == 0xFF5Fu || c == 0xFF60u || c == 0xFFE2u ||
        c == 0xFFE3u || c == 0xFFE4u ||
        (0x1100u <= c && c <= 0x115Fu) || (0x231Au <= c && c <= 0x231Bu) || (0x23E9u <= c && c <= 0x23ECu) ||
        (0x25FDu <= c && c <= 0x25FEu) || (0x2614u <= c && c <= 0x2615u) || (0x2648u <= c && c <= 0x2653u) ||
        (0x26AAu <= c && c <= 0x26ABu) || (0x26BDu <= c && c <= 0x26BEu) || (0x26C4u <= c && c <= 0x26C5u) ||
        (0x26F2u <= c && c <= 0x26F3u) || (0x270Au <= c && c <= 0x270Bu) || (0x2753u <= c && c <= 0x2755u) ||
        (0x2795u <= c && c <= 0x2797u) || (0x2B1Bu <= c && c <= 0x2B1Cu) || (0x2E80u <= c && c <= 0x2E99u) ||
        (0x2E9Bu <= c && c <= 0x2EF3u) || (0x2F00u <= c && c <= 0x2FD5u) || (0x2FF0u <= c && c <= 0x2FFBu) ||
        (0x3001u <= c && c <= 0x3003u) || (0x3012u <= c && c <= 0x3013u) || (0x301Eu <= c && c <= 0x301Fu) ||
        (0x3021u <= c && c <= 0x3029u) || (0x302Au <= c && c <= 0x302Du) || (0x302Eu <= c && c <= 0x302Fu) ||
        (0x3031u <= c && c <= 0x3035u) || (0x3036u <= c && c <= 0x3037u) || (0x3038u <= c && c <= 0x303Au) ||
        (0x3041u <= c && c <= 0x3096u) || (0x3099u <= c && c <= 0x309Au) || (0x309Bu <= c && c <= 0x309Cu) ||
        (0x309Du <= c && c <= 0x309Eu) || (0x30A1u <= c && c <= 0x30FAu) || (0x30FCu <= c && c <= 0x30FEu) ||
        (0x3105u <= c && c <= 0x312Fu) || (0x3131u <= c && c <= 0x318Eu) || (0x3190u <= c && c <= 0x3191u) ||
        (0x3192u <= c && c <= 0x3195u) || (0x3196u <= c && c <= 0x319Fu) || (0x31A0u <= c && c <= 0x31BFu) ||
        (0x31C0u <= c && c <= 0x31E3u) || (0x31F0u <= c && c <= 0x31FFu) || (0x3200u <= c && c <= 0x321Eu) ||
        (0x3220u <= c && c <= 0x3229u) || (0x322Au <= c && c <= 0x3247u) || (0x3251u <= c && c <= 0x325Fu) ||
        (0x3260u <= c && c <= 0x327Fu) || (0x3280u <= c && c <= 0x3289u) || (0x328Au <= c && c <= 0x32B0u) ||
        (0x32B1u <= c && c <= 0x32BFu) || (0x32C0u <= c && c <= 0x32FFu) || (0x3300u <= c && c <= 0x33FFu) ||
        (0x3400u <= c && c <= 0x4DBFu) || (0x4E00u <= c && c <= 0x9FFCu) || (0x9FFDu <= c && c <= 0x9FFFu) ||
        (0xA000u <= c && c <= 0xA014u) || (0xA016u <= c && c <= 0xA48Cu) || (0xA490u <= c && c <= 0xA4C6u) ||
        (0xA960u <= c && c <= 0xA97Cu) || (0xAC00u <= c && c <= 0xD7A3u) || (0xF900u <= c && c <= 0xFA6Du) ||
        (0xFA6Eu <= c && c <= 0xFA6Fu) || (0xFA70u <= c && c <= 0xFAD9u) || (0xFADAu <= c && c <= 0xFAFFu) ||
        (0xFE10u <= c && c <= 0xFE16u) || (0xFE31u <= c && c <= 0xFE32u) || (0xFE33u <= c && c <= 0xFE34u) ||
        (0xFE45u <= c && c <= 0xFE46u) || (0xFE49u <= c && c <= 0xFE4Cu) || (0xFE4Du <= c && c <= 0xFE4Fu) ||
        (0xFE50u <= c && c <= 0xFE52u) || (0xFE54u <= c && c <= 0xFE57u) || (0xFE5Fu <= c && c <= 0xFE61u) ||
        (0xFE64u <= c && c <= 0xFE66u) || (0xFE6Au <= c && c <= 0xFE6Bu) || (0xFF01u <= c && c <= 0xFF03u) ||
        (0xFF05u <= c && c <= 0xFF07u) || (0xFF0Eu <= c && c <= 0xFF0Fu) || (0xFF10u <= c && c <= 0xFF19u) ||
        (0xFF1Au <= c && c <= 0xFF1Bu) || (0xFF1Cu <= c && c <= 0xFF1Eu) || (0xFF1Fu <= c && c <= 0xFF20u) ||
        (0xFF21u <= c && c <= 0xFF3Au) || (0xFF41u <= c && c <= 0xFF5Au) || (0xFFE0u <= c && c <= 0xFFE1u) ||
        (0xFFE5u <= c && c <= 0xFFE6u);
}

size_t removeNewline(char* str) {
    const size_t len = strlen(str);

    if (len > 0 && str[len - 1] == '\n') {
        if (len > 1 && str[len - 2] == '\r') {
            str[len - 2] = '\0';
            return len - 2;
        }

        str[len - 1] = '\0';
        return len - 1;
    }
    return len;
}

int calculateStringWidthUtf8(const char* strUtf8) {
    int width = 0;
    utf8_char_iter iter = make_utf8_char_iter(make_utf8_string(strUtf8));
    // Fallback
    if (!iter.str) return (int)strlen(strUtf8);

    utf8_char c;
    while ((c = next_utf8_char(&iter)).byte_len) {
        const uint32_t code = unicode_code_point(c);
        if (code == '\t') {
            width += TAB_WIDTH;
            continue;
        }

        width += code != 0 && isFullWidth(code) ? 2 : 1;
    }
    return width;
}


FILE* getSourceFile() {
    static FILE* sourceFile = NULL;
    if (!sourceFile) {
        sourceFile = fopen(inputFilePath, "r");
        if (!sourceFile) {
            fprintf(stderr, "謬矣：『%s』未見或無從啟之\n", inputFilePath);
            exit(1);
        }
    }
    return sourceFile;
}

void readErrorLine(FILE* sourceFile, const YYLTYPE* loc, char* buff, const int buffLen) {
    // Move pointer to line
    fseek(sourceFile, loc->line_start_offset, SEEK_SET);

    // Read line from input file.
    if (!fgets(buff, buffLen, sourceFile))
        return;

    removeNewline(buff);
}

bool readNextLine(FILE* sourceFile, char* buff, const int buffLen) {
    if (!fgets(buff, buffLen, sourceFile))
        return false;
    removeNewline(buff);
    return true;
}

void createToken(const YYLTYPE* loc, char* line, char* token) {
    const int tokenLength = loc->last_column_byte - loc->first_column_byte + 1;
    memcpy(token, line + loc->first_column_byte, tokenLength);
    token[tokenLength] = 0;
}

void createTilde(const int width, char* buff, const int buffLen) {
    int i = 0;
    for (i = 0; i < width - 1 && i < buffLen - 1; i++) buff[i] = '~';
    buff[i] = '\0';
}

void printErrorLine(const YYLTYPE* loc) {
    FILE* errorFile = getSourceFile();

    char lineA[ERROR_TEXT_BUFFER_LEN];
    readErrorLine(errorFile, loc, lineA, ERROR_TEXT_BUFFER_LEN);

    // Extract the error token from the line.
    char token[ERROR_TOKEN_BUFFER_LEN];
    createToken(loc, lineA, token);

    // Split line before token
    lineA[loc->first_column_byte] = 0;

    char* lineB = lineA + loc->last_column_byte + 1;

    // calculate width
    const int prefixWidth = calculateStringWidthUtf8(lineA), tokenWidth = calculateStringWidthUtf8(token);

    // Print the error line
    printf(LINE_PREFIX "%s%s%s%s\n", yylineno, lineA, COLOR_RED, token, COLOR_RESET, lineB);

    char tilde[ERROR_TEXT_BUFFER_LEN];
    createTilde(tokenWidth, tilde, ERROR_TEXT_BUFFER_LEN);
    printf(LINE_PREFIX_EMPTY "%*.s%s^%s\n%s", prefixWidth, "", COLOR_RED, tilde, COLOR_RESET);

    // Read additional context
    if (!readNextLine(errorFile, lineA, ERROR_TEXT_BUFFER_LEN))
        return;
    printf("%6d |%s\n", yylineno + 1, lineA);
}

void printTypeErrorLine(const YYLTYPE* locA, const YYLTYPE* locB) {
    FILE* errorFile = getSourceFile();

    char lineA[ERROR_TEXT_BUFFER_LEN];
    readErrorLine(errorFile, locA, lineA, ERROR_TEXT_BUFFER_LEN);

    // Extract the error token from the line.
    char tokenA[ERROR_TOKEN_BUFFER_LEN], tokenB[ERROR_TOKEN_BUFFER_LEN];
    createToken(locA, lineA, tokenA);
    createToken(locB, lineA, tokenB);

    // Split line before token
    lineA[locA->first_column_byte] = 0;

    char* lineB = lineA + locA->last_column_byte + 1;
    lineA[locB->first_column_byte] = 0;

    char* lineC = lineA + locB->last_column_byte + 1;

    // calculate width
    const int prefixWidthA = calculateStringWidthUtf8(lineA), tokenWidthA = calculateStringWidthUtf8(tokenA);
    const int prefixWidthB = calculateStringWidthUtf8(lineB), tokenWidthB = calculateStringWidthUtf8(tokenB);

    // Print the error line
    printf(LINE_PREFIX "%s%s%s%s%s%s%s%s\n", yylineno, lineA, COLOR_RED, tokenA, COLOR_RESET, lineB, COLOR_RED, tokenB, COLOR_RESET, lineC);

    char tilde[ERROR_TEXT_BUFFER_LEN];
    createTilde(tokenWidthA, tilde, ERROR_TEXT_BUFFER_LEN);
    printf(LINE_PREFIX_EMPTY "%*.s%s^%s%s", prefixWidthA, "", COLOR_RED, tilde, COLOR_RESET);

    createTilde(tokenWidthB, tilde, ERROR_TEXT_BUFFER_LEN);
    printf("%*.s%s^%s\n%s", prefixWidthB, "", COLOR_RED, tilde, COLOR_RESET);

    // Read additional context
    if (!readNextLine(errorFile, lineA, ERROR_TEXT_BUFFER_LEN))
        return;
    printf("%6d |%s\n", yylineno + 1, lineA);
}
