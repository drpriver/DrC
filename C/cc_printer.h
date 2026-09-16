#ifndef CC_PRINTER_H
#define CC_PRINTER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/MStringBuilder.h"
#include "cc_type.h"
#include "cc_stmt.h"
#include "cc_expr.h"
#include "cc_parser.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static void cc_print_type(MStringBuilder* sb, CcQualType t);
static void cc_print_statement(MStringBuilder* sb, CcStmtNode*);
static void cc_print_runtime_value(CcParser*, CcQualType, const void*, MStringBuilder*, int indent);
static void cc_print_expr(MStringBuilder* sb, CcExpr* e);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
