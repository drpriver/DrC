#ifndef C_CC_PARSER_C
#define C_CC_PARSER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdio.h>
#include <stdarg.h>
#include "cc_parser.h"
#include "cpp_preprocessor.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
// TODO: rewrite this myself instead of clanker slop

// ---------------------------------------------------------------------------
// Parser helpers
// ---------------------------------------------------------------------------

LOG_PRINTF(3, 4)
static
int
cc_parse_error(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->lexer.cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    return CC_LEX_SYNTAX_ERROR;
}

static
int
cc_next(CcParser* p, CCToken* tok){
    if(p->pending.count){
        *tok = ma_pop(CCToken)(&p->pending);
        return 0;
    }
    return cc_lex_next_token(&p->lexer, tok);
}

static
int
cc_unget(CcParser* p, CCToken* tok){
    return ma_push(CCToken)(&p->pending, p->lexer.cpp.allocator, *tok);
}

static
int
cc_peek(CcParser* p, CCToken* tok){
    int err = cc_next(p, tok);
    if(err) return err;
    return cc_unget(p, tok);
}

static
int
cc_expect_punct(CcParser* p, CCPunct punct){
    CCToken tok;
    int err = cc_next(p, &tok);
    if(err) return err;
    if(tok.type != CC_PUNCTUATOR || tok.punct.punct != punct){
        // Build a readable name for the expected punctuator
        char buf[4];
        int len = 0;
        uint32_t v = (uint32_t)punct;
        // Multi-char puncts are stored as multi-char constants
        if(v > 0xFFFF){
            buf[len++] = (char)(v >> 16);
            buf[len++] = (char)(v >> 8);
            buf[len++] = (char)v;
        } else if(v > 0xFF){
            buf[len++] = (char)(v >> 8);
            buf[len++] = (char)v;
        } else {
            buf[len++] = (char)v;
        }
        buf[len] = 0;
        return cc_parse_error(p, tok.loc, "Expected '%s'", buf);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Expression allocator
// ---------------------------------------------------------------------------

static
CcExpr* _Nullable
cc_alloc_expr(CcParser* p, size_t nvalues){
    size_t size = sizeof(CcExpr) + nvalues * sizeof(CcExpr*);
    return Allocator_zalloc(p->lexer.cpp.allocator, size);
}

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static int cc_parse_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_assignment_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_ternary_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_infix(CcParser* p, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_prefix(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_primary(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_postfix(CcParser* p, CcExpr* operand, CcExpr* _Nullable* _Nonnull out);

// ---------------------------------------------------------------------------
// Precedence table for binary infix operators
// ---------------------------------------------------------------------------

typedef struct {
    CCPunct punct;
    CcExprKind kind;
    int prec;
} BinopEntry;

// Precedences: higher number = tighter binding
static const BinopEntry binop_table[] = {
    {CC_or,      CC_EXPR_LOGOR,  4},
    {CC_and,     CC_EXPR_LOGAND, 5},
    {CC_pipe,    CC_EXPR_BITOR,  6},
    {CC_xor,     CC_EXPR_BITXOR, 7},
    {CC_amp,     CC_EXPR_BITAND, 8},
    {CC_eq,      CC_EXPR_EQ,     9},
    {CC_ne,      CC_EXPR_NE,     9},
    {CC_lt,      CC_EXPR_LT,     10},
    {CC_gt,      CC_EXPR_GT,     10},
    {CC_le,      CC_EXPR_LE,     10},
    {CC_ge,      CC_EXPR_GE,     10},
    {CC_lshift,  CC_EXPR_LSHIFT, 11},
    {CC_rshift,  CC_EXPR_RSHIFT, 11},
    {CC_plus,    CC_EXPR_ADD,    12},
    {CC_minus,   CC_EXPR_SUB,    12},
    {CC_star,    CC_EXPR_MUL,    13},
    {CC_slash,   CC_EXPR_DIV,    13},
    {CC_percent, CC_EXPR_MOD,    13},
};

static
_Bool
binop_lookup(CCPunct punct, CcExprKind* kind, int* prec){
    for(size_t i = 0; i < sizeof binop_table / sizeof binop_table[0]; i++){
        if(binop_table[i].punct == punct){
            *kind = binop_table[i].kind;
            *prec = binop_table[i].prec;
            return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Assignment operator lookup
// ---------------------------------------------------------------------------

static
_Bool
assign_lookup(CCPunct punct, CcExprKind* kind){
    switch((uint32_t)punct){
        case CC_assign:         *kind = CC_EXPR_ASSIGN;       return 1;
        case CC_plus_assign:    *kind = CC_EXPR_ADDASSIGN;    return 1;
        case CC_minus_assign:   *kind = CC_EXPR_SUBASSIGN;    return 1;
        case CC_star_assign:    *kind = CC_EXPR_MULASSIGN;    return 1;
        case CC_slash_assign:   *kind = CC_EXPR_DIVASSIGN;    return 1;
        case CC_percent_assign: *kind = CC_EXPR_MODASSIGN;    return 1;
        case CC_amp_assign:     *kind = CC_EXPR_BITANDASSIGN; return 1;
        case CC_pipe_assign:    *kind = CC_EXPR_BITORASSIGN;  return 1;
        case CC_xor_assign:     *kind = CC_EXPR_BITXORASSIGN; return 1;
        case CC_lshift_assign:  *kind = CC_EXPR_LSHIFTASSIGN; return 1;
        case CC_rshift_assign:  *kind = CC_EXPR_RSHIFTASSIGN; return 1;
        default: return 0;
    }
}

// ---------------------------------------------------------------------------
// Expression parsing
// ---------------------------------------------------------------------------

// comma expression (lowest precedence)
static
int
cc_parse_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcExpr* left;
    int err = cc_parse_assignment_expr(p, &left);
    if(err) return err;
    for(;;){
        CCToken tok;
        err = cc_next(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
            CcExpr* right;
            err = cc_parse_assignment_expr(p, &right);
            if(err) return err;
            CcExpr* node = cc_alloc_expr(p, 1);
            if(!node) return CC_LEX_OOM_ERROR;
            node->kind = CC_EXPR_COMMA;
            node->loc = tok.loc;
            node->value0 = left;
            node->values[0] = right;
            left = node;
        } else {
            cc_unget(p, &tok);
            break;
        }
    }
    *out = left;
    return 0;
}

// assignment expression (right-associative) + ternary
static
int
cc_parse_assignment_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcExpr* left;
    int err = cc_parse_ternary_expr(p, &left);
    if(err) return err;
    CCToken tok;
    err = cc_next(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        CcExprKind kind;
        if(assign_lookup(tok.punct.punct, &kind)){
            CcExpr* right;
            // right-associative: recurse into assignment_expr
            err = cc_parse_assignment_expr(p, &right);
            if(err) return err;
            CcExpr* node = cc_alloc_expr(p, 1);
            if(!node) return CC_LEX_OOM_ERROR;
            node->kind = kind;
            node->loc = tok.loc;
            node->value0 = left;
            node->values[0] = right;
            *out = node;
            return 0;
        }
    }
    cc_unget(p, &tok);
    *out = left;
    return 0;
}

// ternary expression
static
int
cc_parse_ternary_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcExpr* cond;
    // Parse the condition using infix with minimum precedence
    int err = cc_parse_prefix(p, &cond);
    if(err) return err;
    err = cc_parse_infix(p, cond, 4, &cond);
    if(err) return err;
    CCToken tok;
    err = cc_next(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_question){
        CcExpr* then_expr;
        err = cc_parse_expr(p, &then_expr);
        if(err) return err;
        err = cc_expect_punct(p, CC_colon);
        if(err) return err;
        CcExpr* else_expr;
        // right-associative: recurse into ternary
        err = cc_parse_ternary_expr(p, &else_expr);
        if(err) return err;
        CcExpr* node = cc_alloc_expr(p, 2);
        if(!node) return CC_LEX_OOM_ERROR;
        node->kind = CC_EXPR_TERNARY;
        node->loc = tok.loc;
        node->value0 = cond;
        node->values[0] = then_expr;
        node->values[1] = else_expr;
        *out = node;
        return 0;
    }
    cc_unget(p, &tok);
    *out = cond;
    return 0;
}

// Pratt-style infix: left-to-right binary operators
static
int
cc_parse_infix(CcParser* p, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out){
    for(;;){
        CCToken tok;
        int err = cc_next(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        CcExprKind kind;
        int prec;
        if(!binop_lookup(tok.punct.punct, &kind, &prec)){
            cc_unget(p, &tok);
            break;
        }
        if(prec < min_prec){
            cc_unget(p, &tok);
            break;
        }
        CcExpr* right;
        err = cc_parse_prefix(p, &right);
        if(err) return err;
        // Look ahead: if next op has higher precedence, recurse
        err = cc_parse_infix(p, right, prec + 1, &right);
        if(err) return err;
        CcExpr* node = cc_alloc_expr(p, 1);
        if(!node) return CC_LEX_OOM_ERROR;
        node->kind = kind;
        node->loc = tok.loc;
        node->value0 = left;
        node->values[0] = right;
        left = node;
    }
    *out = left;
    return 0;
}

// Prefix unary operators
static
int
cc_parse_prefix(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CCToken tok;
    int err = cc_next(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        CcExprKind kind = CC_EXPR_VALUE;
        _Bool is_prefix = 1;
        switch(tok.punct.punct){
            case CC_minus:     kind = CC_EXPR_NEG;    break;
            case CC_plus:      kind = CC_EXPR_POS;    break;
            case CC_tilde:     kind = CC_EXPR_BITNOT; break;
            case CC_bang:      kind = CC_EXPR_LOGNOT; break;
            case CC_star:      kind = CC_EXPR_DEREF;  break;
            case CC_amp:       kind = CC_EXPR_ADDR;   break;
            case CC_plusplus:   kind = CC_EXPR_PREINC; break;
            case CC_minusminus:kind = CC_EXPR_PREDEC; break;
            default: is_prefix = 0; break;
        }
        if(is_prefix){
            CcExpr* operand;
            err = cc_parse_prefix(p, &operand);
            if(err) return err;
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_LEX_OOM_ERROR;
            node->kind = kind;
            node->loc = tok.loc;
            node->value0 = operand;
            *out = node;
            return 0;
        }
    }
    // Not a prefix op: put it back and parse primary + postfix
    cc_unget(p, &tok);
    CcExpr* primary;
    err = cc_parse_primary(p, &primary);
    if(err) return err;
    return cc_parse_postfix(p, primary, out);
}

// Primary expressions: literals, identifiers, parenthesized
static
int
cc_parse_primary(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CCToken tok;
    int err = cc_next(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_CONSTANT: {
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_LEX_OOM_ERROR;
            node->kind = CC_EXPR_VALUE;
            node->loc = tok.loc;
            switch(tok.constant.ctype){
                case CC_FLOAT:
                    node->float_ = tok.constant.float_value;
                    break;
                case CC_DOUBLE:
                case CC_LONG_DOUBLE:
                    node->double_ = tok.constant.double_value;
                    break;
                default:
                    node->uinteger = tok.constant.integer_value;
                    break;
            }
            *out = node;
            return 0;
        }
        case CC_STRING_LITERAL: {
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_LEX_OOM_ERROR;
            node->kind = CC_EXPR_VALUE;
            node->loc = tok.loc;
            node->str.length = tok.str.length;
            node->text = tok.str.text;
            *out = node;
            return 0;
        }
        case CC_IDENTIFIER: {
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_LEX_OOM_ERROR;
            node->kind = CC_EXPR_IDENTIFIER;
            node->loc = tok.loc;
            node->text = tok.ident.ident->data;
            node->extra = tok.ident.ident->length;
            *out = node;
            return 0;
        }
        case CC_PUNCTUATOR:
            if(tok.punct.punct == CC_lparen){
                CcExpr* inner;
                err = cc_parse_expr(p, &inner);
                if(err) return err;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                *out = inner;
                return 0;
            }
            return cc_parse_error(p, tok.loc, "Unexpected punctuator in expression");
        case CC_KEYWORD:
            if(tok.kw.kw == CC_sizeof){
                // For now, sizeof only on unary expression (not on types)
                // sizeof expr
                CcExpr* operand;
                err = cc_parse_prefix(p, &operand);
                if(err) return err;
                // We can't desugar to a value without types, so use a unary node
                // Reuse CC_EXPR_VALUE with 0 for now — caller can revisit
                // Actually, let's just not handle sizeof yet
                return cc_parse_error(p, tok.loc, "sizeof not yet supported");
            }
            return cc_parse_error(p, tok.loc, "Unexpected keyword in expression");
        case CC_EOF:
            return cc_parse_error(p, tok.loc, "Unexpected end of input in expression");
    }
    return cc_parse_error(p, tok.loc, "Unexpected token in expression");
}

// Postfix operators
static
int
cc_parse_postfix(CcParser* p, CcExpr* operand, CcExpr* _Nullable* _Nonnull out){
    for(;;){
        CCToken tok;
        int err = cc_next(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        switch((uint32_t)tok.punct.punct){
            case CC_plusplus: {
                CcExpr* node = cc_alloc_expr(p, 0);
                if(!node) return CC_LEX_OOM_ERROR;
                node->kind = CC_EXPR_POSTINC;
                node->loc = tok.loc;
                node->value0 = operand;
                operand = node;
                continue;
            }
            case CC_minusminus: {
                CcExpr* node = cc_alloc_expr(p, 0);
                if(!node) return CC_LEX_OOM_ERROR;
                node->kind = CC_EXPR_POSTDEC;
                node->loc = tok.loc;
                node->value0 = operand;
                operand = node;
                continue;
            }
            case CC_lbracket: {
                // subscript: operand[expr]
                CcExpr* index;
                err = cc_parse_expr(p, &index);
                if(err) return err;
                err = cc_expect_punct(p, CC_rbracket);
                if(err) return err;
                CcExpr* node = cc_alloc_expr(p, 1);
                if(!node) return CC_LEX_OOM_ERROR;
                node->kind = CC_EXPR_SUBSCRIPT;
                node->loc = tok.loc;
                node->value0 = operand;
                node->values[0] = index;
                operand = node;
                continue;
            }
            case CC_dot:
            case CC_arrow: {
                CcExprKind mkind = tok.punct.punct == CC_dot ? CC_EXPR_DOT : CC_EXPR_ARROW;
                CCToken member;
                err = cc_next(p, &member);
                if(err) return err;
                if(member.type != CC_IDENTIFIER)
                    return cc_parse_error(p, member.loc, "Expected identifier after '%s'",
                        mkind == CC_EXPR_DOT ? "." : "->");
                // text=member name (in union with value0), values[0]=operand
                CcExpr* mnode = cc_alloc_expr(p, 1);
                if(!mnode) return CC_LEX_OOM_ERROR;
                mnode->kind = mkind;
                mnode->loc = tok.loc;
                mnode->extra = member.ident.ident->length;
                mnode->text = member.ident.ident->data;
                mnode->values[0] = operand;
                operand = mnode;
                continue;
            }
            case CC_lparen: {
                // Function call: operand(args...)
                // Count args by parsing into a temp buffer
                // First check for empty arg list
                CCToken peek;
                err = cc_next(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen){
                    // No args
                    CcExpr* node = cc_alloc_expr(p, 0);
                    if(!node) return CC_LEX_OOM_ERROR;
                    node->kind = CC_EXPR_CALL;
                    node->loc = tok.loc;
                    node->call.nargs = 0;
                    node->value0 = operand;
                    operand = node;
                    continue;
                }
                cc_unget(p, &peek);
                // Parse args into a small stack buffer, then allocate
                CcExpr* arg_buf[64];
                uint32_t nargs = 0;
                for(;;){
                    if(nargs >= 64)
                        return cc_parse_error(p, tok.loc, "Too many function arguments (max 64)");
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, &arg);
                    if(err) return err;
                    arg_buf[nargs++] = arg;
                    CCToken sep;
                    err = cc_next(p, &sep);
                    if(err) return err;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen)
                        break;
                    if(sep.type != CC_PUNCTUATOR || sep.punct.punct != CC_comma)
                        return cc_parse_error(p, sep.loc, "Expected ',' or ')' in function call");
                }
                // nargs values + the function expr in value0
                CcExpr* node = cc_alloc_expr(p, nargs);
                if(!node) return CC_LEX_OOM_ERROR;
                node->kind = CC_EXPR_CALL;
                node->loc = tok.loc;
                node->call.nargs = nargs;
                node->value0 = operand;
                for(uint32_t i = 0; i < nargs; i++)
                    node->values[i] = arg_buf[i];
                operand = node;
                continue;
            }
            default:
                cc_unget(p, &tok);
                goto done;
        }
    }
done:
    *out = operand;
    return 0;
}

// ---------------------------------------------------------------------------
// Expression printer (S-expression format for REPL feedback)
// ---------------------------------------------------------------------------

static
void
cc_print_expr(CcExpr* e){
    switch(e->kind){
        case CC_EXPR_VALUE:
            // Check if it looks like a string (str.length set and text non-null)
            if(e->str.length && e->text){
                printf("\"%.*s\"", e->str.length, e->text);
            } else {
                // Integer/float value — print as integer for simplicity
                printf("%llu", (unsigned long long)e->uinteger);
            }
            return;
        case CC_EXPR_IDENTIFIER:
            printf("%.*s", e->extra, e->text);
            return;
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_STATEMENT_EXPRESSION:
            printf("<unimpl>");
            return;
        // Unary prefix
        case CC_EXPR_NEG:    printf("(- ");     cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_POS:    printf("(+ ");     cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_BITNOT: printf("(~ ");     cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_LOGNOT: printf("(! ");     cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_DEREF:  printf("(* ");     cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_ADDR:   printf("(& ");     cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_PREINC: printf("(++pre "); cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_PREDEC: printf("(--pre "); cc_print_expr(e->value0); printf(")"); return;
        // Unary postfix
        case CC_EXPR_POSTINC: printf("(post++ "); cc_print_expr(e->value0); printf(")"); return;
        case CC_EXPR_POSTDEC: printf("(post-- "); cc_print_expr(e->value0); printf(")"); return;
        // Binary ops
        #define BINOP(K, S) case K: fputs("(" S " ", stdout); cc_print_expr(e->value0); putchar(' '); cc_print_expr(e->values[0]); putchar(')'); return;
        BINOP(CC_EXPR_ADD,    "+")
        BINOP(CC_EXPR_SUB,    "-")
        BINOP(CC_EXPR_MUL,    "*")
        BINOP(CC_EXPR_DIV,    "/")
        BINOP(CC_EXPR_MOD,    "%")
        BINOP(CC_EXPR_BITAND, "&")
        BINOP(CC_EXPR_BITOR,  "|")
        BINOP(CC_EXPR_BITXOR, "^")
        BINOP(CC_EXPR_LSHIFT, "<<")
        BINOP(CC_EXPR_RSHIFT, ">>")
        BINOP(CC_EXPR_LOGAND, "&&")
        BINOP(CC_EXPR_LOGOR,  "||")
        BINOP(CC_EXPR_EQ,     "==")
        BINOP(CC_EXPR_NE,     "!=")
        BINOP(CC_EXPR_LT,     "<")
        BINOP(CC_EXPR_GT,     ">")
        BINOP(CC_EXPR_LE,     "<=")
        BINOP(CC_EXPR_GE,     ">=")
        BINOP(CC_EXPR_ASSIGN,       "=")
        BINOP(CC_EXPR_ADDASSIGN,    "+=")
        BINOP(CC_EXPR_SUBASSIGN,    "-=")
        BINOP(CC_EXPR_MULASSIGN,    "*=")
        BINOP(CC_EXPR_DIVASSIGN,    "/=")
        BINOP(CC_EXPR_MODASSIGN,    "%=")
        BINOP(CC_EXPR_BITANDASSIGN, "&=")
        BINOP(CC_EXPR_BITORASSIGN,  "|=")
        BINOP(CC_EXPR_BITXORASSIGN, "^=")
        BINOP(CC_EXPR_LSHIFTASSIGN, "<<=")
        BINOP(CC_EXPR_RSHIFTASSIGN, ">>=")
        BINOP(CC_EXPR_COMMA,        ",")
        BINOP(CC_EXPR_SUBSCRIPT,    "[]")
        #undef BINOP
        case CC_EXPR_TERNARY:
            printf("(? ");
            cc_print_expr(e->value0);
            printf(" ");
            cc_print_expr(e->values[0]);
            printf(" ");
            cc_print_expr(e->values[1]);
            printf(")");
            return;
        case CC_EXPR_CAST:
            printf("(cast ");
            cc_print_expr(e->value0);
            printf(")");
            return;
        case CC_EXPR_DOT:
            fputs("(. ", stdout);
            cc_print_expr(e->values[0]);
            printf(" %.*s)", e->extra, e->text);
            return;
        case CC_EXPR_ARROW:
            fputs("(-> ", stdout);
            cc_print_expr(e->values[0]);
            printf(" %.*s)", e->extra, e->text);
            return;
        case CC_EXPR_CALL:
            printf("(call ");
            cc_print_expr(e->value0);
            for(uint32_t i = 0; i < e->call.nargs; i++){
                printf(" ");
                cc_print_expr(e->values[i]);
            }
            printf(")");
            return;
    }
    printf("<unknown>");
}

// ---------------------------------------------------------------------------
// Constant expression evaluator
// ---------------------------------------------------------------------------

typedef struct CcEvalResult CcEvalResult;
struct CcEvalResult {
    enum { CC_EVAL_INT, CC_EVAL_UINT, CC_EVAL_FLOAT, CC_EVAL_DOUBLE, CC_EVAL_ERROR } kind;
    union {
        int64_t i;
        uint64_t u;
        float f;
        double d;
    };
};

static CcEvalResult cc_eval_error(void){ return (CcEvalResult){.kind = CC_EVAL_ERROR}; }

// Promote to common type for binary ops
static
void
cc_eval_promote(CcEvalResult* a, CcEvalResult* b){
    // float/double promotions
    if(a->kind == CC_EVAL_DOUBLE || b->kind == CC_EVAL_DOUBLE){
        if(a->kind == CC_EVAL_INT)       { a->d = (double)a->i; a->kind = CC_EVAL_DOUBLE; }
        else if(a->kind == CC_EVAL_UINT) { a->d = (double)a->u; a->kind = CC_EVAL_DOUBLE; }
        else if(a->kind == CC_EVAL_FLOAT){ a->d = (double)a->f; a->kind = CC_EVAL_DOUBLE; }
        if(b->kind == CC_EVAL_INT)       { b->d = (double)b->i; b->kind = CC_EVAL_DOUBLE; }
        else if(b->kind == CC_EVAL_UINT) { b->d = (double)b->u; b->kind = CC_EVAL_DOUBLE; }
        else if(b->kind == CC_EVAL_FLOAT){ b->d = (double)b->f; b->kind = CC_EVAL_DOUBLE; }
        return;
    }
    if(a->kind == CC_EVAL_FLOAT || b->kind == CC_EVAL_FLOAT){
        if(a->kind == CC_EVAL_INT)       { a->f = (float)a->i; a->kind = CC_EVAL_FLOAT; }
        else if(a->kind == CC_EVAL_UINT) { a->f = (float)a->u; a->kind = CC_EVAL_FLOAT; }
        if(b->kind == CC_EVAL_INT)       { b->f = (float)b->i; b->kind = CC_EVAL_FLOAT; }
        else if(b->kind == CC_EVAL_UINT) { b->f = (float)b->u; b->kind = CC_EVAL_FLOAT; }
        return;
    }
    // unsigned promotion
    if(a->kind == CC_EVAL_UINT || b->kind == CC_EVAL_UINT){
        if(a->kind == CC_EVAL_INT) { a->u = (uint64_t)a->i; a->kind = CC_EVAL_UINT; }
        if(b->kind == CC_EVAL_INT) { b->u = (uint64_t)b->i; b->kind = CC_EVAL_UINT; }
    }
}

static
int64_t
cc_eval_to_int(CcEvalResult r){
    switch(r.kind){
        case CC_EVAL_INT:    return r.i;
        case CC_EVAL_UINT:   return (int64_t)r.u;
        case CC_EVAL_FLOAT:  return (int64_t)r.f;
        case CC_EVAL_DOUBLE: return (int64_t)r.d;
        default: return 0;
    }
}

static
CcEvalResult
cc_eval_expr(CcExpr* e){
    switch(e->kind){
        case CC_EXPR_VALUE:
            if(e->str.length && e->text)
                return cc_eval_error(); // can't eval strings to a number
            return (CcEvalResult){.kind = CC_EVAL_UINT, .u = e->uinteger};
        case CC_EXPR_IDENTIFIER:
            return cc_eval_error(); // unresolved identifier
        case CC_EXPR_NEG: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            switch(v.kind){
                case CC_EVAL_INT:    v.i = -v.i; return v;
                case CC_EVAL_UINT:   return (CcEvalResult){.kind = CC_EVAL_INT, .i = -(int64_t)v.u};
                case CC_EVAL_FLOAT:  v.f = -v.f; return v;
                case CC_EVAL_DOUBLE: v.d = -v.d; return v;
                default: return cc_eval_error();
            }
        }
        case CC_EXPR_POS: return cc_eval_expr(e->value0);
        case CC_EXPR_BITNOT: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            switch(v.kind){
                case CC_EVAL_INT:  v.i = ~v.i; return v;
                case CC_EVAL_UINT: v.u = ~v.u; return v;
                default: return cc_eval_error();
            }
        }
        case CC_EXPR_LOGNOT: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            int64_t iv = cc_eval_to_int(v);
            return (CcEvalResult){.kind = CC_EVAL_INT, .i = !iv};
        }
        case CC_EXPR_COMMA: {
            cc_eval_expr(e->value0); // discard
            return cc_eval_expr(e->values[0]);
        }
        case CC_EXPR_TERNARY: {
            CcEvalResult cond = cc_eval_expr(e->value0);
            if(cond.kind == CC_EVAL_ERROR) return cond;
            if(cc_eval_to_int(cond))
                return cc_eval_expr(e->values[0]);
            else
                return cc_eval_expr(e->values[1]);
        }
        // Binary arithmetic
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL:
        case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
        case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:
        {
            CcEvalResult L = cc_eval_expr(e->value0);
            CcEvalResult R = cc_eval_expr(e->values[0]);
            if(L.kind == CC_EVAL_ERROR || R.kind == CC_EVAL_ERROR)
                return cc_eval_error();
            cc_eval_promote(&L, &R);
            #define IBINOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:    return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; \
                    case CC_EVAL_UINT:   return (CcEvalResult){.kind=CC_EVAL_UINT, .u=L.u op R.u}; \
                    case CC_EVAL_FLOAT:  return (CcEvalResult){.kind=CC_EVAL_FLOAT, .f=L.f op R.f}; \
                    case CC_EVAL_DOUBLE: return (CcEvalResult){.kind=CC_EVAL_DOUBLE, .d=L.d op R.d}; \
                    default: return cc_eval_error(); \
                }
            #define IINTOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:  return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; \
                    case CC_EVAL_UINT: return (CcEvalResult){.kind=CC_EVAL_UINT, .u=L.u op R.u}; \
                    default: return cc_eval_error(); \
                }
            #define ICMPOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:    return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; \
                    case CC_EVAL_UINT:   return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.u op R.u}; \
                    case CC_EVAL_FLOAT:  return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.f op R.f}; \
                    case CC_EVAL_DOUBLE: return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.d op R.d}; \
                    default: return cc_eval_error(); \
                }
            switch(e->kind){
                case CC_EXPR_ADD: IBINOP(+)
                case CC_EXPR_SUB: IBINOP(-)
                case CC_EXPR_MUL: IBINOP(*)
                case CC_EXPR_DIV:
                    // Check for division by zero
                    if(L.kind == CC_EVAL_INT && R.i == 0) return cc_eval_error();
                    if(L.kind == CC_EVAL_UINT && R.u == 0) return cc_eval_error();
                    IBINOP(/)
                case CC_EXPR_MOD:
                    if(L.kind == CC_EVAL_INT && R.i == 0) return cc_eval_error();
                    if(L.kind == CC_EVAL_UINT && R.u == 0) return cc_eval_error();
                    IINTOP(%)
                case CC_EXPR_BITAND: IINTOP(&)
                case CC_EXPR_BITOR:  IINTOP(|)
                case CC_EXPR_BITXOR: IINTOP(^)
                case CC_EXPR_LSHIFT: IINTOP(<<)
                case CC_EXPR_RSHIFT: IINTOP(>>)
                case CC_EXPR_LOGAND:
                    return (CcEvalResult){.kind=CC_EVAL_INT, .i=cc_eval_to_int(L) && cc_eval_to_int(R)};
                case CC_EXPR_LOGOR:
                    return (CcEvalResult){.kind=CC_EVAL_INT, .i=cc_eval_to_int(L) || cc_eval_to_int(R)};
                case CC_EXPR_EQ: ICMPOP(==)
                case CC_EXPR_NE: ICMPOP(!=)
                case CC_EXPR_LT: ICMPOP(<)
                case CC_EXPR_GT: ICMPOP(>)
                case CC_EXPR_LE: ICMPOP(<=)
                case CC_EXPR_GE: ICMPOP(>=)
                default: return cc_eval_error();
            }
            #undef IBINOP
            #undef IINTOP
            #undef ICMPOP
        }
        default:
            return cc_eval_error();
    }
}

static
void
cc_print_eval_result(CcEvalResult r){
    switch(r.kind){
        case CC_EVAL_INT:    printf("%lld", (long long)r.i); break;
        case CC_EVAL_UINT:   printf("%llu", (unsigned long long)r.u); break;
        case CC_EVAL_FLOAT:  printf("%g", (double)r.f); break;
        case CC_EVAL_DOUBLE: printf("%g", r.d); break;
        case CC_EVAL_ERROR:  fputs("<cannot evaluate>", stdout); break;
    }
}

// ---------------------------------------------------------------------------
// Top-level parsing
// ---------------------------------------------------------------------------

static
int
cc_parse_top_level(CcParser* p, _Bool* finished){
    CCToken tok;
    int err = cc_next(p, &tok);
    if(err) return err;
    if(tok.type == CC_EOF){
        *finished = 1;
        return 0;
    }
    *finished = 0;
    // In REPL mode, parse expression statements
    if(p->repl){
        // Check for empty statement
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_semi)
            return 0;
        // Push back and parse expression
        cc_unget(p, &tok);
        CcExpr* expr;
        err = cc_parse_expr(p, &expr);
        if(err) return err;
        err = cc_expect_punct(p, CC_semi);
        if(err) return err;
        cc_print_expr(expr);
        CcEvalResult result = cc_eval_expr(expr);
        if(result.kind != CC_EVAL_ERROR){
            fputs(" = ", stdout);
            cc_print_eval_result(result);
        }
        putchar('\n');
        fflush(stdout);
        return 0;
    }
    // Non-REPL: not yet implemented
    return cc_parse_error(p, tok.loc, "Declarations not yet implemented");
}

static
void
cc_parser_discard_input(CcParser* p){
    p->pending.count = 0;
    cpp_discard_all_input(&p->lexer.cpp);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
