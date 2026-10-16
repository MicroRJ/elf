//
//	See Copyright Notice In elf.h
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parser.h"

#include "logging.c"
#include "lexer.c"
#include "ast.c"

#define I64_MAX_MAGNITUDE 0x7fffffffffffffffull
#define I64_MIN_MAGNITUDE 0x8000000000000000ull

static Ast parse_unary_expr(Parser *parser);
static Ast parse_expr(Parser *parser);
static Ast parse_subexpr(Parser *parser, u32 rank);
static Ast parse_postfix_expr(Parser *parser);
static Ast parse_table_expr(Parser *parser);
static Ast parse_array_expr(Parser *parser);
static Ast parse_ident_expr(Parser *parser);
static Ast parse_call_argument(Parser *parser);
static Ast parse_stat(Parser *parser);
static Ast parse_if_stat(Parser *parser);
static Ast parse_json_ast(Parser *parser);
static Ast parse_json_ast_value(Parser *parser);
static Ast parse_call_expr(Parser *parser, Ast callee, SourceSite site);
static Token consume_token(Parser *parser);
static Token take_token(Parser *parser, Token_Type type);

static Ast ast_from_token_error(Token token)
{
	ASSERT(token_is_error(token));
	return ast_from_error(token.error);
}

static Token take_field_name(Parser *parser)
{
	Token token = parser->tok;
	if (token.type == TOK_IDENTIFIER || (token.type >= TOK_JSON && token.type <= TOK_GLOBAL))
	{
		consume_token(parser);
		return token;
	}
	return take_token(parser, TOK_IDENTIFIER);
}


static AstType binary_ast_expr_type_from_token_type(Token_Type type)
{
	switch (type)
	{
		case TOK_ELLIPSIS:  return AST_RANGE;
		case TOK_LOG_AND:   return AST_AND;
		case TOK_LOG_OR:    return AST_OR;
		case TOK_NIL_OR:    return AST_NIL_OR;
		case TOK_NIL_AND:   return AST_NIL_AND;
		case TOK_ADD:       return AST_ADD;
		case TOK_SUB:       return AST_SUB;
		case TOK_DIV:       return AST_DIV;
		case TOK_MUL:       return AST_MUL;
		case TOK_POW:       return AST_POW;
		case TOK_MOD:       return AST_MOD;
		case TOK_NEQ:       return AST_NOT_EQ;
		case TOK_EQ:        return AST_EQ;
		case TOK_GT:        return AST_GREATER_THAN;
		case TOK_GTEQ:      return AST_GREATER_THAN_EQ;
		case TOK_LT:        return AST_LESS_THAN;
		case TOK_LTEQ:      return AST_LESS_THAN_EQ;
		case TOK_SHL:       return AST_SHIFT_LEFT;
		case TOK_SHR:       return AST_SHIFT_RIGHT;
		case TOK_BIT_XOR:   return AST_BITWISE_XOR;
		case TOK_BIT_OR:    return AST_BITWISE_OR;
		case TOK_BIT_AND:   return AST_BITWISE_AND;
		default:            return AST_NONE;
	}
}

static AstType compound_assign_ast_type_from_token_type(Token_Type type)
{
	switch (type)
	{
		case TOK_ADD_ASSIGN: return AST_ADD_ASSIGN;
		case TOK_SUB_ASSIGN: return AST_SUB_ASSIGN;
		case TOK_MUL_ASSIGN: return AST_MUL_ASSIGN;
		case TOK_DIV_ASSIGN: return AST_DIV_ASSIGN;
		case TOK_MOD_ASSIGN: return AST_MOD_ASSIGN;
		case TOK_XOR_ASSIGN: return AST_XOR_ASSIGN;
		case TOK_SHL_ASSIGN: return AST_SHL_ASSIGN;
		case TOK_SHR_ASSIGN: return AST_SHR_ASSIGN;
		default:             return AST_NONE;
	}
}

static AstType precedence_from_binary_ast_expr_type(AstType type)
{
	switch (type)
	{
		case AST_POW:             return 13 + 1;
		case AST_MUL:
		case AST_DIV:
		case AST_MOD:             return 12 + 1;
		case AST_ADD:
		case AST_SUB:             return 11 + 1;
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:     return 10 + 1;
		case AST_LESS_THAN:
		case AST_LESS_THAN_EQ:
		case AST_GREATER_THAN:
		case AST_GREATER_THAN_EQ: return  9 + 1;
		case AST_EQ:
		case AST_NOT_EQ:          return  8 + 1;
		case AST_BITWISE_AND:     return  7 + 1;
		case AST_BITWISE_XOR:     return  6 + 1;
		case AST_BITWISE_OR:      return  5 + 1;
		case AST_AND:
		case AST_NIL_AND:         return  4 + 1;
		case AST_OR:
		case AST_NIL_OR:          return  3 + 1;
		case AST_RANGE:           return  2 + 1;
		default:                  return  0;
	}
}

// TODO(RJ): audit these and use globally defined compiler message codes.
typedef enum
{
	ERROR_NONE = 0,
	INTERNAL_ERROR_INVALID_TREE,
	ERROR_UNEXPECTED_TOKEN,
	ERROR_INVALID_EXPRESSION,
	ERROR_EXPECTED_TOKEN,
	ERROR_NOT_IN_A_LOOP,
	ERROR_UNREACHABLE_STAT,
	ERROR_INVALID_CONTRACT_NAME,
	ERROR_INVALID_DECL,
	ERROR_SYNTAX_DEPRECATION,
	ERROR_INVALID_FIELD_INITIALIZER,
	ERROR_MISS_COMMA,
	ERROR_EXCESS_COMMA,
	ERROR_MISSING_FUNCTION_BODY,
	ERROR_INVALID_STORE_CLOSURE_VALUE,
	ERROR_INVALID_STORE_METAFIELD,
	ERROR_INVALID_STORE,
	// ERROR_UNREFERENCED_ENTITY,
	ERROR_EXCESS_RVALUE,
	ERROR_INVALID_STATEMENT,
}
Error;

static Com_Error parser_report(Parser *parser, Com_Severity severity, Error error, SourceSite site, const char *message)
{
	(void)error;

	if (!source_slice_is_valid(site)) {
		site = parser->tok.site;
	}

	b32 reported = compiler_report(parser->compiler, severity, COMPILER_PHASE_PARSER, site, "%s", message);
	return reported ? COM_ERROR_SOURCE : COM_ERROR_OOM;
}

static Ast parser_error(Parser *parser, Error error, SourceSite site, const char *message)
{
	return ast_from_error(parser_report(parser, COMPILER_ERROR, error, site, message));
}

static Ast parser_errorf(Parser *parser, Error error, SourceSite site, const char *format, ...)
{
	(void)error;
	if (!source_slice_is_valid(site)) {
		site = parser->tok.site;
	}
	va_list args;
	va_start(args, format);
	b32 reported = compiler_reportv(parser->compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, format, args);
	va_end(args);
	return ast_from_error(reported ? COM_ERROR_SOURCE : COM_ERROR_OOM);
}

static Ast parser_unexpected_token(Parser *parser, Token token)
{
	return parser_errorf(parser, ERROR_UNEXPECTED_TOKEN, token.site
	,	"unexpected token '%s'", token_type_name(token.type));
}

static Token parser_expected_token(Parser *parser, Token token, Token_Type expected)
{
	Ast error = parser_errorf(parser, ERROR_EXPECTED_TOKEN, token.site
	,	"expected '%s', got '%s'", token_type_name(expected), token_type_name(token.type));
	return token_from_error(error->error, token.site);
}

static Ast parser_invalid_table_entry(Parser *parser, SourceSite site)
{
	return parser_error(parser, ERROR_INVALID_FIELD_INITIALIZER, site, "invalid table entry");
}

Parser *parser_create(Compiler *compiler)
{
	ASSERT(compiler != 0);
	Parser *parser = elf_arena_push_zero(compiler->arena, sizeof(*parser));
	if (!parser) {
		return 0;
	}
	parser->compiler = compiler;
	parser->atoms = compiler->atoms;
	parser->result_arena = compiler->arena;
	parser->scratch_arena = elf_arena_create(0);
	if (!parser->scratch_arena.data) {
		return 0;
	}
	return parser;
}

void parser_destroy(Parser *parser)
{
	if (parser) {
		elf_arena_destroy(&parser->scratch_arena);
	}
}

b32 parser_set_source(Parser *parser, Com_SourceId source_id)
{
	ASSERT(parser);
	ASSERT(source_id < parser->compiler->source_count);
	Com_Source *source = parser->compiler->sources + source_id;
	ASSERT(source->name != 0);
	ASSERT(source->source.data != 0);

	elf_arena_reset(&parser->scratch_arena);
	parser->source_id = source_id;
	parser->tok = (Token) {};
	parser->tok_prev = (Token) {};
	parser->tok_prox = (Token) {};
	parser->ast_stack_size = 4096;
	parser->ast_stack_index = 0;
	parser->ast_stack = elf_arena_push_zero(&parser->scratch_arena, sizeof(*parser->ast_stack) * parser->ast_stack_size);
	if (!parser->ast_stack) {
		return false;
	}

	lexer_init(&parser->lexer, parser->compiler, source_id, &parser->scratch_arena);
	consume_token(parser);
	consume_token(parser);
	return true;
}

static Token consume_token(Parser *par)
{
	Token token = token_is_error(par->tok_prox) ? par->tok_prox : lex_token(&par->lexer);
	par->tok_prev = par->tok;
	par->tok = par->tok_prox;
	par->tok_prox = token;
	return par->tok_prev;
}

static inline b32 peek_token(Parser *parser, Token_Type type)
{
	return parser->tok.type == type;
}

static inline b32 pick_token(Parser *parser, Token_Type type)
{
	if (peek_token(parser, type)) {
		consume_token(parser);
		return true;
	}
	return false;
}

static Token take_token(Parser *parser, Token_Type type)
{
	Token tok = parser->tok;

	if (token_is_error(tok)) {
		return tok;
	}
	if (!pick_token(parser, type)) {
		return parser_expected_token(parser, tok, type);
	}

	return tok;
}

static inline b32 peek_next_token(Parser *parser, Token_Type type)
{
	return parser->tok_prox.type == type;
}

static b32 push_ast(Parser *par, Ast tree)
{
	if (par->ast_stack_index == par->ast_stack_size)
	{
		u32 stack_size = par->ast_stack_size ? par->ast_stack_size * 2 : 256;
		Ast *stack = elf_arena_push(&par->scratch_arena, sizeof(*stack) * stack_size);
		if (!stack) {
			return false;
		}
		if (par->ast_stack_index) {
			copy_memory(stack, par->ast_stack, sizeof(*stack) * par->ast_stack_index);
		}
		par->ast_stack = stack;
		par->ast_stack_size = stack_size;
	}
	par->ast_stack[par->ast_stack_index++] = tree;
	return true;
}

static AstArray pop_ast_array(Parser *par, u32 count)
{
	ASSERT(par->ast_stack_index >= count);
	par->ast_stack_index -= count;
	AstArray result = {.count = count};
	result.items = elf_arena_push_copy(par->result_arena, sizeof(*result.items) * count, par->ast_stack + par->ast_stack_index);
	return result;
}

static Ast parse_file(Parser *parser)
{
	if (token_is_error(parser->tok)) {
		return ast_from_token_error(parser->tok);
	}
	Token tok = parser->tok;
	u32 nstats = 0;
	while (!peek_token(parser, TOK_NONE))
	{
		if (token_is_error(parser->tok)) {
			return ast_from_token_error(parser->tok);
		}
		Token before = parser->tok;
		Ast_T *stat = parse_stat(parser);
		if (ast_is_error(stat)) {
			return stat;
		}
		if (!stat)
		{
			return parser_unexpected_token(parser, before);
		}
		if (before.site.offset == parser->tok.site.offset && before.type == parser->tok.type)
		{
			return parser_error(parser, ERROR_INVALID_STATEMENT, before.site, "parser made no progress while reading statement");
		}

		if (!push_ast(parser, stat)) {
			return ERROR_OOM_AST;
		}
		++ nstats;
	}

	AstArray stats = pop_ast_array(parser, nstats);
	if (stats.count && !stats.items) {
		return ERROR_OOM_AST;
	}
	Ast body = create_block_ast(parser, tok.site, stats);

	Ast file_ast = create_file_ast(parser, tok.site, body);
	return file_ast;
}

// <function> ::= ("fun" | "->") ["(" [<parameter-list>] ")"] <statement>
// <parameter-list> ::= <parameter> {"," <parameter>} ["," "..."] | "..."
// <parameter> ::= <identifier> [":" <identifier>] ["=" <expression>]
static Ast parse_function(Parser *par, Token marker)
{
	ASSERT(marker.type == TOK_FUN || marker.type == TOK_ARROW);

	u32 stack_start = par->ast_stack_index;
	u32 param_count = 0;
	AstArray params = {};
	b32 variadic = false;
	Ast error = 0;
	if (pick_token(par, TOK_LEFT_PAREN))
	{
		if (!peek_token(par, TOK_PAREN_RIGHT)) do
		{
			Token tok = par->tok;
			if (pick_token(par, TOK_ELLIPSIS))
			{
				variadic = true;
				if (pick_token(par, TOK_COMMA)) {
					error = parser_error(par, ERROR_INVALID_EXPRESSION, tok.site, "variadic marker must be the final parameter");
				}
			}
			else
			{
				Ast name = parse_ident_expr(par);
				Ast type = 0;
				Ast expr = 0;
				if (!ast_is_error(name) && pick_token(par, TOK_COLON)) {
					type = parse_ident_expr(par);
				}
				if (!ast_is_error(name) && !ast_is_error(type) && pick_token(par, TOK_ASSIGN)) {
					expr = parse_expr(par);
				}
				Ast param = create_param_ast(par, tok.site, name, type, expr);
				if (ast_is_error(param)) {
					error = param;
				}
				else if (!push_ast(par, param)) {
					error = ERROR_OOM_AST;
				}
				else {
					++param_count;
				}
			}
		} while (!error && pick_token(par, TOK_COMMA));

		if (!error)
		{
			Token right_paren = take_token(par, TOK_PAREN_RIGHT);
			if (token_is_error(right_paren)) {
				error = ast_from_token_error(right_paren);
			}
		}
		if (!error && param_count)
		{
			params = pop_ast_array(par, param_count);
			if (!params.items) {
				error = ERROR_OOM_AST;
			}
		}
	}
	if (error)
	{
		par->ast_stack_index = stack_start;
		return error;
	}

	Ast body = parse_stat(par);
	if (ast_is_error(body)) {
		return body;
	}

	if (!body) {
		return parser_error(par, ERROR_MISSING_FUNCTION_BODY, par->tok.site, "expected function body");
	}

	Ast function = create_function_ast(par, marker.site, params, variadic, body);
	return function;
}

static b32 token_ends_expression(Token_Type type)
{
	switch (type)
	{
		case TOK_NONE:
		case TOK_QMARK:
		case TOK_COMMA:
		case TOK_SEMICOLON:
		case TOK_PAREN_RIGHT:
		case TOK_RIGHT_BRACE:
		case TOK_SQUARE_RIGHT:
		case TOK_THEN:
		case TOK_ELSE:
		case TOK_ELIF:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

static Ast parse_tuple_expr(Parser *par)
{
	Token tok = par->tok;
	u32 nargs = 0;
	do
	{
		Ast expr = parse_expr(par);
		if (ast_is_error(expr)) {
			return expr;
		}
		if (!push_ast(par, expr)) {
			return ERROR_OOM_AST;
		}
		++ nargs;
	}
	while (pick_token(par, TOK_COMMA));

	Ast tuple = 0;
	if (nargs)
	{
		AstArray args = pop_ast_array(par, nargs);
		if (!args.items) {
			return ERROR_OOM_AST;
		}
		tuple = create_tuple_ast(par, tok.site, args);
	}
	return tuple;
}

static Ast parse_optional_tuple_expr(Parser *par)
{
	if (token_ends_expression(par->tok.type)) {
		return NULL_AST;
	}
	return parse_tuple_expr(par);
}

static Ast parse_for_identifier_tuple(Parser *parser)
{
	Token start = parser->tok;
	u32 stack_start = parser->ast_stack_index;
	u32 nargs = 0;

	do
	{
		Token token = take_token(parser, TOK_IDENTIFIER);
		if (token_is_error(token)) {
			parser->ast_stack_index = stack_start;
			return ast_from_token_error(token);
		}

		Ast name = create_ident_ast(parser, token.site, token.atom);
		if (ast_is_error(name)) {
			parser->ast_stack_index = stack_start;
			return name;
		}
		if (!push_ast(parser, name)) {
			parser->ast_stack_index = stack_start;
			return ERROR_OOM_AST;
		}
		++ nargs;
	}
	while (pick_token(parser, TOK_COMMA));

	AstArray names = pop_ast_array(parser, nargs);
	if (!names.items) {
		return ERROR_OOM_AST;
	}
	return create_tuple_ast(parser, start.site, names);
}

static Ast parse_get_mem_expr(Parser *parser)
{
	Token tok = take_token(parser, TOK_M_GET_MEM);
	if (token_is_error(tok)) {
		return ast_from_token_error(tok);
	}

	Token opening = take_token(parser, TOK_LEFT_PAREN);
	if (token_is_error(opening)) {
		return ast_from_token_error(opening);
	}
	Ast expr = parse_expr(parser);
	if (ast_is_error(expr)) {
		return expr;
	}
	Token closing = take_token(parser, TOK_PAREN_RIGHT);
	if (token_is_error(closing)) {
		return ast_from_token_error(closing);
	}

	return create_get_mem_ast(parser, tok.site, expr);
}

static Ast parse_ident_expr(Parser *par)
{
	Token tok = take_token(par, TOK_IDENTIFIER);
	if (token_is_error(tok)) {
		return ast_from_token_error(tok);
	}
	return create_ident_ast(par, tok.site, tok.atom);
}

static Ast parse_positive_integer_literal(Parser *parser, Token token)
{
	if (token.integer_magnitude > I64_MAX_MAGNITUDE)
	{
		return parser_errorf(parser, ERROR_INVALID_EXPRESSION, token.site
		,	"integer literal out of range for i64: %llu"
		,	token.integer_magnitude);
	}

	return create_int_ast(parser, token.site, (i64)token.integer_magnitude);
}

static Ast parse_negative_integer_literal(Parser *parser, Token sign, Token token)
{
	if (token.integer_magnitude > I64_MIN_MAGNITUDE)
	{
		return parser_errorf(parser, ERROR_INVALID_EXPRESSION, token.site
		,	"integer literal out of range for i64: -%llu"
		,	token.integer_magnitude);
	}

	i64 value = 0;
	if (token.integer_magnitude == I64_MIN_MAGNITUDE) {
		value = -9223372036854775807LL - 1LL;
	}
	else {
		value = -(i64)token.integer_magnitude;
	}

	return create_int_ast(parser, sign.site, value);
}

static Ast parse_interpolated_string(Parser *parser)
{
	Token start = take_token(parser, TOK_STRING_START);
	if (token_is_error(start)) {
		return ast_from_token_error(start);
	}
	u32 stack_start = parser->ast_stack_index;
	u32 nparts = 0;

	Ast first_part = create_atom_ast(parser, start.site, start.atom);
	if (ast_is_error(first_part)) {
		return first_part;
	}
	if (!push_ast(parser, first_part)) {
		return ERROR_OOM_AST;
	}
	nparts += 1;

	for (;;)
	{
		Ast expr = parse_expr(parser);
		if (ast_is_error(expr))
		{
			parser->ast_stack_index = stack_start;
			return expr;
		}
		if (!expr)
		{
			parser->ast_stack_index = stack_start;
			return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected expression inside interpolated string");
		}
		if (!push_ast(parser, expr)) {
			parser->ast_stack_index = stack_start;
			return ERROR_OOM_AST;
		}
		nparts += 1;

		Token part = parser->tok;
		if (part.type != TOK_STRING_PART && part.type != TOK_STRING_END)
		{
			parser->ast_stack_index = stack_start;
			if (token_is_error(part)) {
				return ast_from_token_error(part);
			}
			return parser_error(parser, ERROR_EXPECTED_TOKEN, part.site, "expected the end of an interpolated expression");
		}

		consume_token(parser);
		Ast text = create_atom_ast(parser, part.site, part.atom);
		if (ast_is_error(text)) {
			parser->ast_stack_index = stack_start;
			return text;
		}
		if (!push_ast(parser, text)) {
			parser->ast_stack_index = stack_start;
			return ERROR_OOM_AST;
		}
		nparts += 1;

		if (part.type == TOK_STRING_END) {
			break;
		}
	}

	AstArray parts = pop_ast_array(parser, nparts);
	if (!parts.items) {
		return ERROR_OOM_AST;
	}
	return create_interpolated_string_ast(parser, start.site, parts);
}

static Ast parse_unary_expr(Parser *parser)
{
	Token tok = parser->tok;

	Ast value = 0;
	switch (tok.type)
	{
		case TOK_ERROR:
		{
			value = ast_from_token_error(tok);
		}
		break;
		case TOK_ELLIPSIS:
		{
			consume_token(parser);
			value = create_ellipsis_ast(parser, tok.site);
		}
		break;
		case TOK_IDENTIFIER:
		{
			value = parse_ident_expr(parser);
		}
		break;
		case TOK_NIL:
		{
			consume_token(parser);
			value = create_nil_ast(parser, tok.site);
		}
		break;
		case TOK_TRUE:
		{
			consume_token(parser);
			value = create_int_ast(parser, tok.site, 1);
		}
		break;
		case TOK_FALSE:
		{
			consume_token(parser);
			value = create_int_ast(parser, tok.site, 0);
		}
		break;
		case TOK_LETTER:
		case TOK_INTEGER:
		{
			consume_token(parser);
			value = parse_positive_integer_literal(parser, tok);
		}
		break;
		case TOK_NUMBER:
		{
			consume_token(parser);
			value = create_num_ast(parser, tok.site, tok.number);
		}
		break;
		case TOK_STRING:
		{
			consume_token(parser);
			value = create_atom_ast(parser, tok.site, tok.atom);
		}
		break;
		case TOK_STRING_START:
		{
			value = parse_interpolated_string(parser);
		}
		break;
		case TOK_IMPORT:
		{
			consume_token(parser);
			Token path = parser->tok;
			if (token_is_error(path))
			{
				value = ast_from_token_error(path);
			}
			else if (path.type == TOK_STRING)
			{
				consume_token(parser);
				value = create_import_ast(parser, tok.site, path.atom);
			}
			else
			{
				value = parser_error(parser, ERROR_EXPECTED_TOKEN, path.site, "expected a plain string after 'import'");
			}
		}
		break;
		case TOK_TILDE:
		{
			consume_token(parser);
			value = parse_subexpr(parser, 10000);
			if (!ast_is_error(value)) {
				value = create_unary_ast(parser, tok.site, AST_BITWISE_NOT, value);
			}
		}
		break;
		case TOK_MUL:
		{
			consume_token(parser);
			value = parse_subexpr(parser, 10000);
			if (!ast_is_error(value))
			{
				value = create_measure_ast(parser, tok.site, value);
			}
		}
		break;
		case TOK_SUB:
		{
			consume_token(parser);
			if (peek_token(parser, TOK_INTEGER))
			{
				Token integer = consume_token(parser);
				value = parse_negative_integer_literal(parser, tok, integer);
			}
			else if (peek_token(parser, TOK_NUMBER))
			{
				Token number = consume_token(parser);
				value = create_num_ast(parser, tok.site, -number.number);
			}
			else
			{
				value = parse_subexpr(parser, 10000);
				if (!ast_is_error(value)) {
					value = create_binary_ast(parser, tok.site, AST_SUB, create_int_ast(parser, tok.site, 0), value);
				}
			}
		}
		break;
		case TOK_ADD:
		{
			consume_token(parser);
			value = parse_subexpr(parser, 10000);
		}
		break;
		case TOK_JSON:
		{
			consume_token(parser);
			value = parse_json_ast_value(parser);
		}
		break;
		case TOK_LEFT_BRACE:
		{
			value = parse_table_expr(parser);
		}
		break;
		case TOK_SQUARE_LEFT:
		{
			value = parse_array_expr(parser);
		}
		break;

		case TOK_LEFT_PAREN:
		{
			consume_token(parser);
			if (peek_token(parser, TOK_PAREN_RIGHT))
			{
				value = parser_error(parser, ERROR_INVALID_EXPRESSION, tok.site, "expected expression inside parentheses");
			}
			else
			{
				value = parse_expr(parser);
			}
			if (!ast_is_error(value))
			{
				Token closing = take_token(parser, TOK_PAREN_RIGHT);
				if (token_is_error(closing)) {
					value = ast_from_token_error(closing);
				}
			}
		}
		break;
		case TOK_FUN:
		{
			value = parse_function(parser, consume_token(parser));
		}
		break;
		case TOK_RECURSE:
		{
			consume_token(parser);
			value = create_nullary_ast(parser, tok.site, AST_RECURSE);
		}
		break;
		case TOK_M_GET_MEM:
		{
			value = parse_get_mem_expr(parser);
		}
		break;

		default:
		{
			value = parser_unexpected_token(parser, tok);
		}
		break;
	}

	esc:
	return value;
}

static Ast parse_table_entry(Parser *parser)
{
	Token start = parser->tok;
	Ast key = NULL_AST;
	Ast value = NULL_AST;

	if (peek_token(parser, TOK_IDENTIFIER) && peek_next_token(parser, TOK_ASSIGN))
	{
		Token name = consume_token(parser);
		Token assign = take_token(parser, TOK_ASSIGN);
		if (token_is_error(assign)) {
			return ast_from_token_error(assign);
		}

		key = create_atom_ast(parser, name.site, name.atom);
		if (ast_is_error(key)) {
			return key;
		}
		value = parse_expr(parser);
	}
	else if (pick_token(parser, TOK_SQUARE_LEFT))
	{
		key = parse_expr(parser);
		if (ast_is_error(key)) {
			return key;
		}
		Token closing = take_token(parser, TOK_SQUARE_RIGHT);
		if (token_is_error(closing)) {
			return ast_from_token_error(closing);
		}
		Token assign = take_token(parser, TOK_ASSIGN);
		if (token_is_error(assign)) {
			return ast_from_token_error(assign);
		}

		value = parse_expr(parser);
	}
	else
	{
		key = parse_expr(parser);
		if (ast_is_error(key))
		{
			return key;
		}
		Token assign = take_token(parser, TOK_ASSIGN);
		if (token_is_error(assign))
		{
			return ast_from_token_error(assign);
		}
		value = parse_expr(parser);
	}

	if (ast_is_error(value)) {
		return value;
	}
	if (ast_is_error(key)) {
		return key;
	}
	if (!key || !value)
	{
		return parser_invalid_table_entry(parser, start.site);
	}

	return create_table_entry_ast(parser, start.site, key, value);
}

static Ast parse_table_expr(Parser *parser)
{
	Token table_token = take_token(parser, TOK_LEFT_BRACE);
	if (token_is_error(table_token)) {
		return ast_from_token_error(table_token);
	}

	u32 nargs = 0;
	while (!peek_token(parser, TOK_NONE) && !peek_token(parser, TOK_RIGHT_BRACE))
	{
		Ast entry = parse_table_entry(parser);
		if (ast_is_error(entry)) {
			return entry;
		}

		if (!push_ast(parser, entry)) {
			return ERROR_OOM_AST;
		}
		++ nargs;

		pick_token(parser, TOK_COMMA);
	}

	Token closing = take_token(parser, TOK_RIGHT_BRACE);
	if (token_is_error(closing)) {
		return ast_from_token_error(closing);
	}

	AstArray args = pop_ast_array(parser, nargs);
	if (args.count && !args.items) {
		return ERROR_OOM_AST;
	}
	Ast table = create_table_ast(parser, table_token.site, args);
	return table;
}

static Ast parse_array_expr(Parser *parser)
{
	Token array_token = take_token(parser, TOK_SQUARE_LEFT);
	if (token_is_error(array_token))
	{
		return ast_from_token_error(array_token);
	}

	u32 count = 0;
	while (!peek_token(parser, TOK_NONE) && !peek_token(parser, TOK_SQUARE_RIGHT))
	{
		Ast item = parse_expr(parser);
		if (ast_is_error(item))
		{
			return item;
		}
		if (!item)
		{
			return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected array item");
		}
		if (!push_ast(parser, item))
		{
			return ERROR_OOM_AST;
		}
		count += 1;
		pick_token(parser, TOK_COMMA);
	}

	Token closing = take_token(parser, TOK_SQUARE_RIGHT);
	if (token_is_error(closing))
	{
		return ast_from_token_error(closing);
	}

	AstArray items = pop_ast_array(parser, count);
	if (items.count && !items.items)
	{
		return ERROR_OOM_AST;
	}
	return create_array_ast(parser, array_token.site, items);
}

static Ast parse_call_expr(Parser *par, Ast callee, SourceSite site)
{
	u32 stack_start = par->ast_stack_index;
	u32 nargs = 0;

	if (peek_token(par, TOK_LEFT_BRACE))
	{
		Com_Error warning = parser_report(par, COMPILER_WARNING, ERROR_SYNTAX_DEPRECATION, par->tok.site, "implicit table call syntax is deprecated; write 'expr({...})'");
		if (warning == COM_ERROR_OOM) {
			return ERROR_OOM_AST;
		}
		Ast expr = parse_table_expr(par);
		if (ast_is_error(expr)) {
			return expr;
		}
		if (!expr) {
			return parser_error(par, ERROR_INVALID_EXPRESSION, par->tok.site, "expected call argument");
		}

		if (!push_ast(par, expr)) {
			return ERROR_OOM_AST;
		}
		++ nargs;
	}
	else if (pick_token(par, TOK_LEFT_PAREN))
	{
		pick_token(par, TOK_COMMA);

		if (!peek_token(par, TOK_PAREN_RIGHT))
		{
			do
			{
				if (peek_token(par, TOK_PAREN_RIGHT))
				{
					par->ast_stack_index = stack_start;
					return parser_error(par, ERROR_EXCESS_COMMA, par->tok_prev.site, "excess comma");
				}
				if (peek_token(par, TOK_COMMA))
				{
					par->ast_stack_index = stack_start;
					return parser_error(par, ERROR_EXCESS_COMMA, par->tok.site, "excess comma");
				}

				Ast x = parse_call_argument(par);
				if (ast_is_error(x)) {
					par->ast_stack_index = stack_start;
					return x;
				}
				if (!x) {
					par->ast_stack_index = stack_start;
					return parser_error(par, ERROR_INVALID_EXPRESSION, par->tok.site, "expected call argument");
				}

				if (!push_ast(par, x)) {
					par->ast_stack_index = stack_start;
					return ERROR_OOM_AST;
				}
				++ nargs;
			}
			while (pick_token(par, TOK_COMMA));
		}

		Token closing = take_token(par, TOK_PAREN_RIGHT);
		if (token_is_error(closing)) {
			par->ast_stack_index = stack_start;
			return ast_from_token_error(closing);
		}
	}

	AstArray args = pop_ast_array(par, nargs);
	if (args.count && !args.items) {
		return ERROR_OOM_AST;
	}
	return create_call_ast(par, site, callee, args, 0);
}

static Ast parse_call_argument(Parser *parser)
{
	if (peek_token(parser, TOK_IDENTIFIER) && peek_next_token(parser, TOK_ASSIGN))
	{
		Token name_token = consume_token(parser);
		Token assign = take_token(parser, TOK_ASSIGN);
		if (token_is_error(assign)) {
			return ast_from_token_error(assign);
		}

		Ast name = create_ident_ast(parser, name_token.site, name_token.atom);
		if (ast_is_error(name)) {
			return name;
		}
		Ast expr = parse_expr(parser);
		if (ast_is_error(expr)) {
			return expr;
		}
		if (!expr) {
			return parser_error(parser, ERROR_INVALID_EXPRESSION, assign.site, "expected named argument expression");
		}
		return create_named_argument_ast(parser, name_token.site, name, expr);
	}

	return parse_expr(parser);
}

static Ast parse_field_postfix(Parser *par, Ast left)
{
	Token tok = take_token(par, TOK_DOT);
	if (token_is_error(tok)) {
		return ast_from_token_error(tok);
	}

	if (pick_token(par, TOK_LEFT_PAREN))
	{
		u32 nargs = 0;
		do
		{
			Token name = take_field_name(par);
			if (token_is_error(name)) {
				return ast_from_token_error(name);
			}
			Ast right = create_atom_ast(par, name.site, name.atom);
			Ast value = create_field_ast(par, tok.site, left, right);
			if (ast_is_error(value)) {
				return value;
			}
			if (!push_ast(par, value)) {
				return ERROR_OOM_AST;
			}
			++ nargs;
		}
		while (pick_token(par, TOK_COMMA));

		AstArray args = pop_ast_array(par, nargs);
		if (!args.items) {
			return ERROR_OOM_AST;
		}
		left = create_tuple_ast(par, tok.site, args);
		if (ast_is_error(left)) {
			return left;
		}
		Token closing = take_token(par, TOK_PAREN_RIGHT);
		if (token_is_error(closing)) {
			return ast_from_token_error(closing);
		}
	}
	else if (pick_token(par, TOK_LEFT_BRACE))
	{
		return parser_error(par, ERROR_INVALID_EXPRESSION, tok.site, "field table projection is not implemented");
	}
	else
	{
		Token name = take_field_name(par);
		if (token_is_error(name)) {
			return ast_from_token_error(name);
		}
		Ast right = create_atom_ast(par, name.site, name.atom);
		left = create_field_ast(par, tok.site, left, right);
	}
	return left;
}

// index_postfix ::= "[" index_expr ("," index_expr)* "]"
// index_expr    ::= expr | expr? "..." expr?
// Comma-separated indexes are chained: value[x, y] is value[x][y].
static Ast parse_index_postfix(Parser *par, Ast value)
{
	Token tok = take_token(par, TOK_SQUARE_LEFT);
	if (token_is_error(tok)) {
		return ast_from_token_error(tok);
	}

	do
	{
		Ast index = parse_expr(par);
		if (ast_is_error(index)) {
			return index;
		}
		if (!index) {
			return parser_error(par, ERROR_INVALID_EXPRESSION, par->tok.site, "expected index expression");
		}
		if (index->kind == AST_ELLIPSIS)
		{
			Ast right = 0;
			if (!peek_token(par, TOK_SQUARE_RIGHT) && !peek_token(par, TOK_COMMA))
			{
				right = parse_expr(par);
				if (ast_is_error(right)) {
					return right;
				}
				if (!right) {
					return parser_error(par, ERROR_INVALID_EXPRESSION, par->tok.site, "expected range expression");
				}
			}
			index = create_binary_ast(par, index->site, AST_RANGE, 0, right);
			if (ast_is_error(index)) {
				return index;
			}
		}

		if (index->kind == AST_TUPLE)
		{
			return parser_error(par, ERROR_INVALID_EXPRESSION, index->site, "tuple indexes are not implemented");
		}
		else if (index->kind == AST_RANGE)
		{
			value = create_range_index_ast(par, tok.site, value, index);
			if (ast_is_error(value)) {
				return value;
			}
		}
		else
		{
			value = create_index_ast(par, tok.site, value, index);
			if (ast_is_error(value)) {
				return value;
			}
		}
	}
	while (pick_token(par, TOK_COMMA));

	Token closing = take_token(par, TOK_SQUARE_RIGHT);
	if (token_is_error(closing)) {
		return ast_from_token_error(closing);
	}
	return value;
}

static Ast parse_postfix_expr(Parser *parser)
{
	Token tok = parser->tok;

	Ast value = parse_unary_expr(parser);
	if (ast_is_error(value)) goto esc;

	while (parser->tok.type != TOK_NONE && !parser->tok.line_break_before)
	{
		tok = parser->tok;

		switch (tok.type)
		{
			case TOK_DOT:
			{
				value = parse_field_postfix(parser, value);
			}
			break;

			case TOK_SQUARE_LEFT:
			{
				value = parse_index_postfix(parser, value);
			}
			break;

			case TOK_COLON:
			{
				consume_token(parser);
				Token name = take_field_name(parser);
				if (token_is_error(name)) {
					value = ast_from_token_error(name);
				}
				else
				{
					Ast y = create_atom_ast(parser, name.site, name.atom);
					value = create_meta_field_ast(parser, tok.site, value, y);
				}
			}
			break;

			case TOK_LEFT_BRACE:
			case TOK_LEFT_PAREN:
			{
				value = parse_call_expr(parser, value, tok.site);
			}
			break;

			case TOK_ARROW:
			{
				consume_token(parser);
				Ast function = parse_function(parser, tok);
				value = create_carg_call_ast(parser, tok.site, value, function);
			}
			break;

			default: goto esc;
		}
		if (ast_is_error(value)) {
			goto esc;
		}
	}

	esc:
	return value;
}

static Ast parse_subexpr(Parser *parser, u32 upper_precedence)
{
	Token tok = parser->tok;

	Ast x = parse_postfix_expr(parser);
	if (ast_is_error(x)) goto esc;

	for (;;)
	{
		tok = parser->tok;
		if (tok.line_break_before) break;

		AstType ast_type = binary_ast_expr_type_from_token_type(tok.type);
		if (ast_type == AST_NONE) break;

		u32 inner_precedence = precedence_from_binary_ast_expr_type(ast_type);
		if (inner_precedence <= upper_precedence) break;

		consume_token(parser);

		Ast y = 0;
		if (ast_type != AST_RANGE ||
		(!peek_token(parser, TOK_SQUARE_RIGHT) && !peek_token(parser, TOK_COMMA)))
		{
			y = parse_subexpr(parser, inner_precedence);
		}
		if (ast_is_error(y)) {
			x = y;
			break;
		}
		if (!y && ast_type != AST_RANGE)
		{
			x = parser_error(parser, ERROR_INVALID_EXPRESSION, tok.site, "expected expression after binary operator");
			break;
		}

		x = create_binary_ast(parser, tok.site, ast_type, x, y);
		if (ast_is_error(x)) {
			break;
		}
	}

	esc:
	return x;
}

static Ast parse_expr(Parser *parser)
{
	return parse_subexpr(parser, 0);
}

static Ast parse_expr_stat(Parser *parser, b32 exported)
{
	Ast x = parse_tuple_expr(parser);
	if (ast_is_error(x)) {
		return x;
	}
	if (!x) {
		return parser_unexpected_token(parser, parser->tok);
	}

	Token tok = parser->tok;
	AstType compound_assign_type = compound_assign_ast_type_from_token_type(tok.type);
	if (compound_assign_type != AST_NONE)
	{
		consume_token(parser);

		Ast y = parse_tuple_expr(parser);
		if (ast_is_error(y)) {
			return y;
		}
		if (!y) {
			return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected expression after assignment operator");
		}
		return create_binary_ast(parser, tok.site, compound_assign_type, x, y);
	}

	switch (parser->tok.type)
	{
		case TOK_STATIC_BIND:
		{
			consume_token(parser);

			if (x->tuple.count != 1 || x->tuple.items[0]->kind != AST_IDENT)
			{
				return parser_error(parser, ERROR_INVALID_STATEMENT, x->site, "static declaration name must be an identifier");
			}

			Ast y = parse_expr(parser);
			if (ast_is_error(y)) {
				return y;
			}
			if (!y) {
				return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected static declaration expression");
			}
			x = create_static_decl_ast(parser, tok.site, x->tuple.items[0], y, exported);
		}
		break;
		case TOK_BIND:
		case TOK_HARD_BIND:
		{
			consume_token(parser);

			Ast y = parse_tuple_expr(parser);
			if (ast_is_error(y)) {
				return y;
			}
			if (!y) {
				return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected declaration expression");
			}
			u32 tags = tok.type == TOK_HARD_BIND ? AST_DECL_TAG_CONSTANT : AST_DECL_TAG_NONE;
			x = create_decl_ast(parser, tok.site, tags, x, y);
		}
		break;
		case TOK_ASSIGN:
		{
			consume_token(parser);

			Ast y = parse_tuple_expr(parser);
			if (ast_is_error(y)) {
				return y;
			}
			if (!y) {
				return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected assignment expression");
			}
			x = create_assign_ast(parser, tok.site, x, y);
		}
		break;
		case TOK_NIL_ASSIGN:
		{
			consume_token(parser);

			Ast y = parse_tuple_expr(parser);
			if (ast_is_error(y)) {
				return y;
			}
			if (!y) {
				return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected nil assignment expression");
			}
			x = create_binary_ast(parser, tok.site, AST_NIL_ASSIGN, x, y);
		}
		break;
		default:
		{
		}
		break;
	}
	return x;
}

static Ast parse_stat(Parser *parser)
{
	Token tok = parser->tok;
	Ast tree = 0;

	switch (tok.type)
	{
		case TOK_ERROR:
		{
			return ast_from_token_error(tok);
		}

		case TOK_NONE:
		case TOK_RIGHT_BRACE:
		case TOK_PAREN_RIGHT:
		case TOK_THEN:
		case TOK_ELSE:
		case TOK_ELIF:
		{
		}
		break;

		case TOK_DEFER:
		{
			consume_token(parser);
			tree = parse_stat(parser);
			if (ast_is_error(tree)) {
				return tree;
			}
			if (!tree) {
				return parser_error(parser, ERROR_INVALID_STATEMENT, parser->tok.site, "expected deferred statement");
			}
			tree = create_defer_ast(parser, tok.site, tree);
		}
		break;

		case TOK_EXPORT:
		{
			consume_token(parser);
			tree = parse_expr_stat(parser, true);
			if (ast_is_error(tree)) {
				return tree;
			}
			if (!tree || tree->kind != AST_STATIC_DECL) {
				return parser_error(parser, ERROR_INVALID_STATEMENT, tok.site, "'export' must be followed by a static declaration");
			}
		}
		break;

		case TOK_RET:
		{
			consume_token(parser);

			Ast expr = 0;
			if (!parser->tok.line_break_before)
			{
				expr = parse_optional_tuple_expr(parser);
				if (ast_is_error(expr)) {
					return expr;
				}
			}
			tree = create_return_ast(parser, tok.site, expr);
		}
		break;

		case TOK_BREAK:
		{
			consume_token(parser);
			Ast expr = 0;
			if (!parser->tok.line_break_before)
			{
				expr = parse_optional_tuple_expr(parser);
				if (ast_is_error(expr)) {
					return expr;
				}
			}
			tree = create_break_ast(parser, tok.site, expr);
		}
		break;

		case TOK_CONTINUE:
		{
			consume_token(parser);
			Ast expr = 0;
			if (!parser->tok.line_break_before)
			{
				expr = parse_optional_tuple_expr(parser);
				if (ast_is_error(expr)) {
					return expr;
				}
			}
			tree = create_continue_ast(parser, tok.site, expr);
		}
		break;

		case TOK_LEFT_BRACE:
		{
			u32 nstats = 0;
			consume_token(parser);
			while (!peek_token(parser, TOK_NONE) && !peek_token(parser, TOK_RIGHT_BRACE))
			{
				Token before = parser->tok;
				Ast_T *stat = parse_stat(parser);
				if (ast_is_error(stat)) {
					return stat;
				}
				if (!stat)
				{
					return parser_unexpected_token(parser, before);
				}
				if (before.site.offset == parser->tok.site.offset && before.type == parser->tok.type)
				{
					return parser_error(parser, ERROR_INVALID_STATEMENT, before.site, "parser made no progress while reading block statement");
				}

				if (!push_ast(parser, stat)) {
					return ERROR_OOM_AST;
				}
				++ nstats;
			}
			Token closing = take_token(parser, TOK_RIGHT_BRACE);
			if (token_is_error(closing)) {
				return ast_from_token_error(closing);
			}

			AstArray stats = pop_ast_array(parser, nstats);
			if (stats.count && !stats.items) {
				return ERROR_OOM_AST;
			}
			tree = create_block_ast(parser, tok.site, stats);
		}
		break;

		case TOK_WHILE:
		{
			consume_token(parser);

			Ast pred = parse_expr(parser);
			if (ast_is_error(pred)) {
				return pred;
			}
			if (!pred) {
				return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected while predicate");
			}

			Token question = take_token(parser, TOK_QMARK);
			if (token_is_error(question)) {
				return ast_from_token_error(question);
			}

			Ast body = parse_stat(parser);
			if (ast_is_error(body)) {
				return body;
			}
			if (!body) {
				return parser_error(parser, ERROR_INVALID_STATEMENT, parser->tok.site, "expected while body");
			}

			tree = create_while_ast(parser, tok.site, pred, body);
		}
		break;

		case TOK_FOR:
		{
			consume_token(parser);

			Ast name = parse_for_identifier_tuple(parser);
			if (ast_is_error(name)) {
				return name;
			}
			if (!name) {
				return parser_error(parser, ERROR_INVALID_DECL, parser->tok.site, "expected for-loop declaration");
			}
			u32 decl_tags = AST_DECL_TAG_NONE;
			if (!pick_token(parser, TOK_BIND))
			{
				Token hard_bind = take_token(parser, TOK_HARD_BIND);
				if (token_is_error(hard_bind)) {
					return ast_from_token_error(hard_bind);
				}
				decl_tags |= AST_DECL_TAG_CONSTANT;
			}

			Ast init_expr = parse_tuple_expr(parser);
			if (ast_is_error(init_expr)) {
				return init_expr;
			}
			if (!init_expr) {
				return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected for-loop initializer");
			}
			Ast init = create_decl_ast(parser, tok.site, decl_tags, name, init_expr);
			if (ast_is_error(init)) {
				return init;
			}

			if (pick_token(parser, TOK_QMARK))
			{
				if (init_expr->tuple.count != 1)
				{
					return parser_error(parser, ERROR_INVALID_EXPRESSION, init_expr->site, "range for loops require exactly one expression on the right-hand side");
				}

				Ast range = init_expr->tuple.items[0];
				if (!range || (range->kind != AST_RANGE && range->kind != AST_RANGE_INDEX))
				{
					return parser_error(parser, ERROR_INVALID_EXPRESSION, init_expr->site, "range for loops require a range or ranged index expression");
				}

				Ast body = parse_stat(parser);
				if (ast_is_error(body)) {
					return body;
				}
				if (!body) {
					return parser_error(parser, ERROR_INVALID_STATEMENT, parser->tok.site, "expected for-loop body");
				}

				tree = create_for_range_ast(parser, tok.site, init, body);
			}
			else
			{
				Token first_separator = take_token(parser, TOK_SEMICOLON);
				if (token_is_error(first_separator)) {
					return ast_from_token_error(first_separator);
				}

				Ast pred = parse_expr(parser);
				if (ast_is_error(pred)) {
					return pred;
				}
				if (!pred) {
					return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected for-loop predicate");
				}
				Token second_separator = take_token(parser, TOK_SEMICOLON);
				if (token_is_error(second_separator)) {
					return ast_from_token_error(second_separator);
				}

				Ast step = parse_expr_stat(parser, false);
				if (ast_is_error(step)) {
					return step;
				}
				if (!step) {
					return parser_error(parser, ERROR_INVALID_STATEMENT, parser->tok.site, "expected for-loop step");
				}
				Token question = take_token(parser, TOK_QMARK);
				if (token_is_error(question)) {
					return ast_from_token_error(question);
				}

				Ast body = parse_stat(parser);
				if (ast_is_error(body)) {
					return body;
				}
				if (!body) {
					return parser_error(parser, ERROR_INVALID_STATEMENT, parser->tok.site, "expected for-loop body");
				}

				tree = create_for_ast(parser, tok.site, init, pred, step, body);
			}
		}
		break;

		case TOK_IF:
		{
			consume_token(parser);
			tree = parse_if_stat(parser);
		}
		break;

		default:
		{
			tree = parse_expr_stat(parser, false);
		}
		break;
	}

	return tree;
}

static Ast parse_if_stat(Parser *parser)
{
	Token tok = parser->tok;
	Ast pred = parse_expr(parser);
	if (ast_is_error(pred)) {
		return pred;
	}
	if (!pred) {
		return parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site, "expected if predicate");
	}
	if (!pick_token(parser, TOK_QMARK)) {
		return parser_error(parser, ERROR_EXPECTED_TOKEN, pred->site, "expected '?' for 'if' statement");
	}

	Ast true_clause = parse_stat(parser);
	if (ast_is_error(true_clause)) {
		return true_clause;
	}
	if (!true_clause) {
		return parser_error(parser, ERROR_INVALID_STATEMENT, parser->tok.site, "expected if body");
	}

	Ast else_clause = 0;
	if (pick_token(parser, TOK_ELIF)) {
		else_clause = parse_if_stat(parser);
	}
	else if (pick_token(parser, TOK_ELSE)) {
		else_clause = parse_stat(parser);
	}
	if (ast_is_error(else_clause)) {
		return else_clause;
	}

	Ast stat = create_if_ast(parser, tok.site, pred, true_clause, else_clause);
	return stat;
}

static Ast parse_constexpr(Parser *parser)
{
	Ast ast = parse_tuple_expr(parser);
	if (ast_is_error(ast)) {
		return ast;
	}
	if (!peek_token(parser, TOK_NONE))
	{
		return parser_unexpected_token(parser, parser->tok);
	}
	return ast;
}

#include "parse_json.c"

Ast parser_parse_file(Parser *parser)
{
	return parse_file(parser);
}

Ast parser_parse_constant_expr(Parser *parser)
{
	return parse_constexpr(parser);
}

Ast parser_parse_json(Parser *parser)
{
	return parse_json_ast(parser);
}

Ast parser_parse_jsonl(Parser *parser)
{
	return parse_jsonl_ast(parser);
}

