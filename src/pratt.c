#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#if PRATT_DEBUG
# define ARENA_DEBUG 1
#else
# define ARENA_DEBUG 0
#endif
#include "arena.h"

typedef uint64_t u64;
typedef int64_t  i64;
typedef int32_t  i32;

#define ArrSize(arr) ((sizeof(arr)/sizeof(arr[0])))

#define OPERATORS \
X(TOK_ERR, 0, "[ERROR]")\
X(TOK_NUM, 0, "num")\
X(TOK_PLUS, 2, "+")\
X(TOK_MINUS, 2, "-")\
X(TOK_ASTERISK, 3, "*")\
X(TOK_FORWARD_SLASH, 3, "/")\
X(TOK_OPEN_PAREN, 0, "(")\
X(TOK_CLOSE_PAREN, 0, ")")\
X(TOK_EOF, 0, "eof")\

typedef enum TokenKind {
#define X(tok, prec, print) tok,
OPERATORS
#undef X
} TokenKind;

const i32 tok_prec_table[] = {
#define X(tok, prec, print) prec,
OPERATORS
#undef X
};

const char* tok_printable[] = {
#define X(tok, prec, print) print,
OPERATORS
#undef X
};

typedef struct Token {
  i64 kind;
  double value;
  i64 pos;
}Token;


typedef enum {
  ERR_NONE = 0,
  ERR_UNEXPECTED_TOKEN,
  ERR_DIVIDE_BY_ZERO,
} ParseError;

typedef struct {
  char* ptr;
  i64 len;
} String8;


bool is_binary_operator(Token tok);

#define Str8Lit(cstr) {.ptr=cstr,.len=(sizeof(cstr))-1}
#define Str8Fmt(str8) (int)str8.len, str8.ptr

String8 input_right = Str8Lit("1 + 2 + 3+4");
String8 input_left = Str8Lit("4 * 3 + 2 - 1");
String8 input_mixed = Str8Lit("1 + 2 + 3 + 4 + 5");
String8 input_paren = Str8Lit("1 + 2 + 3 + (4 + 5)");
String8 input_paren2 = Str8Lit("(1+2)*(3*4.0)");
String8 input_paren3 = Str8Lit("(((1 + 2)))");
String8 input_paren4 = Str8Lit("((1 + 2) * (3 + 4))*(4)-1");
String8 input_minus = Str8Lit("-1 + 2");
String8 input_zero = Str8Lit("1 / 0");

typedef struct AstNode AstNode;
struct AstNode {
  i64 kind;
  AstNode* left;
  AstNode* right;
  double value;
};

typedef struct Lexer {
  Token tok;
  Token prev_tok;
  String8 stream;
  i64 pos;
} Lexer;

Lexer lexer_init(String8 in)
{
  return (Lexer){.pos = 0, .stream = in, .tok = (Token){.kind = -1, .pos = -1}, .prev_tok = {}};
}

char lex_peek_char(Lexer* l)
{
  Assert(l->pos <= l->stream.len);
  if (l->pos >= l->stream.len) return -1;
  return l->stream.ptr[l->pos];
}

void lex_eat_char(Lexer* l)
{
  l->pos += 1;
  if (l->pos > l->stream.len) l->pos = l->stream.len;
  Assert(l->pos <= l->stream.len);
}

bool is_whitespace(char c)
{
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void skip_whitespace(Lexer* l)
{
  for (;;)
  {
    char c = lex_peek_char(l);
    if (is_whitespace(c)) lex_eat_char(l);
    else break;
  }
}

bool is_num(char c)
{
  return (c >= '0' && c <= '9');
}

String8 scan_number(Lexer* l)
{
  String8 result = l->stream;
  result.len = 0;
  result.ptr = l->stream.ptr + l->pos;
  for (;;)
  {
    char c = lex_peek_char(l);
    if (is_num(c) || c == '.' || (c == '-' && result.len == 0))
    {
      lex_eat_char(l);
      result.len += 1;
    }
    else break;
  }
  return result;
}

bool try_parse_num(String8 str, double* num)
{
  bool result = 1;
  char* endptr = NULL;
  double temp = strtod(str.ptr, &endptr);
  if (endptr != (str.ptr + str.len))
  {
    result = 0;
  }
  else
  {
    *num = temp;
  }
  return result;
}

Token lex_peek_token(Lexer* l)
{
  if (l->tok.kind != -1) return l->tok;
  Token* tok = &l->tok;
  *tok = (Token){.kind = TOK_ERR, .pos = l->pos};
  char c = -1;
  skip_whitespace(l);
  c = lex_peek_char(l);
  switch (c)
  {
    case -1:
      {
        tok->kind = TOK_EOF;
      } break;
    case '+':
      {
        tok->kind = TOK_PLUS;
        lex_eat_char(l);
      } break;
    case '-':
      {
        lex_eat_char(l);
        c = lex_peek_char(l);
        if (is_num(c))
        {
          // if the previous token was an operator it's a unary minus
          // @Hack: and also it it's the start of input it's also a unary minus
          if (is_binary_operator(l->prev_tok) || l->pos == 1)
          {
            l->pos -= 1;
            goto number;
          }
        }
        tok->kind = TOK_MINUS;
      } break;
    case '*':
      {
        tok->kind = TOK_ASTERISK;
        lex_eat_char(l);
      } break;
    case '/':
      {
        tok->kind = TOK_FORWARD_SLASH;
        lex_eat_char(l);
      } break;
    case '(':
      {
        tok->kind = TOK_OPEN_PAREN;
        lex_eat_char(l);
      } break;
    case ')':
      {
        tok->kind = TOK_CLOSE_PAREN;
        lex_eat_char(l);
      } break;
    default:
      {
        if (is_num(c))
        {
number:
          String8 num_str = scan_number(l);
          if (num_str.ptr == NULL) goto err;
          double num = 0.0;
          if (!try_parse_num(num_str, &num))
          {
            tok->kind = TOK_ERR;
          }
          else
          {
            tok->kind = TOK_NUM;
            tok->value = num;
          }
        }
        else
        {
err:
          tok->kind = TOK_ERR;
        }
      } break;
  }
  return l->tok;
}

// Token lex_peek_prev_token(Lexer* l)
// {
//   Assert(l->pos-1 > 0);
//   return l->stream[l->pos-1];
// }

void lex_eat_token(Lexer* l)
{
  l->prev_tok = l->tok;
  l->tok = (Token){.kind = -1};
}

typedef struct AstStackNode AstStackNode;
struct AstStackNode{
  AstNode* node;
  AstStackNode* prev;
};

typedef struct AstStack{
  AstStackNode* top;
}AstStack;

#define StackIsEmpty(s) (s->top == NULL)
#define StackPush(s, n) do{ \
  n->prev = s->top; \
  s->top = n; \
 }while(0)

bool is_binary_operator(Token tok)
{
  return (tok.kind > TOK_NUM) && (tok.kind < TOK_OPEN_PAREN);
}

AstNode* make_number(Arena* a, double val)
{
  AstNode* node = arena_push_struct(a, AstNode);
  node->kind = TOK_NUM;
  node->value = val;
  return node;
}

AstNode* make_operator(Arena* a, TokenKind kind)
{
  AstNode* node = arena_push_struct(a, AstNode);
  node->kind = kind;
  node->value = 0.0;
  return node;
}

/*
parse_expression(min_prec)
{
  left = parse_leaf()
  while 1
  {
    next = get_next_token()
    if !is_binary_operator(next) node = left

    next_prec = get_precedence(next)
    if next_prec <= min_prec
    {
      node = left
    }
    else
    {
      right = parse_expression(next_prec)
      node = make_binary(left, to_operator(next), right)
    }
    if node == left break

    left = node
  }
  return node // ???
}

*/

AstNode* parse_leaf(Arena* a, Lexer* l)
{
  Token tok = lex_peek_token(l);
  lex_eat_token(l);
  Assert(tok.kind == TOK_NUM);
  return make_number(a, tok.value);
}

AstNode* parse_expression_rec(Arena* a, Lexer* l, ParseError* err, i32 min_prec)
{
  AstNode* root = NULL;
  AstNode* left = NULL;
  Token tok = {0};

  for (;;)
  {
    if (*err != ERR_NONE) break;
    tok = lex_peek_token(l);
    if (tok.kind == TOK_NUM)
    {
      left = make_number(a, tok.value);
      root = left;
      lex_eat_token(l);
      // break;
    }
    else if (tok.kind == TOK_OPEN_PAREN)
    {
      lex_eat_token(l);
      left = parse_expression_rec(a, l, err, -999);
      root = left;
      tok = lex_peek_token(l);
      Assert(tok.kind == TOK_CLOSE_PAREN);
      lex_eat_token(l);
    }
    else if (tok.kind == TOK_CLOSE_PAREN)
    {
      break;
    }
    else if (is_binary_operator(tok))
    {
      i32 next_prec = tok_prec_table[tok.kind];

      if (next_prec > min_prec)
      {
        lex_eat_token(l);
        root = make_operator(a, tok.kind);
        root->left = left;
        root->right = parse_expression_rec(a, l, err, next_prec);
      }
      else
      {
        root = left;
        break;
      }
      left = root;
    }
    else if (tok.kind == TOK_EOF)
    {
      root = left;
      break;
    }
    else
    {
      *err = ERR_UNEXPECTED_TOKEN;
    }
  }

  return root;
}

AstNode* parse_expression(Arena* a, Lexer* l, ParseError* err)
{
  return parse_expression_rec(a, l, err, -9999);
}


double eval_expression_recursive(AstNode* root)
{
  if (root->kind == TOK_NUM) return root->value;
  double lhs = eval_expression_recursive(root->left);
  double rhs = eval_expression_recursive(root->right);
  if (root->kind == TOK_PLUS) return lhs + rhs;
  if (root->kind == TOK_MINUS) return lhs - rhs;
  if (root->kind == TOK_ASTERISK) return lhs * rhs;
  if (root->kind == TOK_FORWARD_SLASH) return lhs / rhs; // it's not invalid to divide by  floating zero but still not sure if its correct
  Assert(0);
  return 0.0;
}

double eval_string(Arena* a, String8 input)
{
  double result = 0.0;
  Lexer l = lexer_init(input);
  ParseError err = ERR_NONE;
  AstNode* parse_res = parse_expression(a, &l, &err);
  if (err == ERR_NONE)
  {
    result = eval_expression_recursive(parse_res);
  }
  return result;
}

void test_lex_string(String8 input)
{
  Lexer l = lexer_init(input);
  for (;;)
  {
    Token tok = lex_peek_token(&l);
    if (tok.kind == TOK_NUM)
    {
      printf("%s = %lf\n", tok_printable[tok.kind], tok.value);
    }
    else
    {
      printf("%s\n", tok_printable[tok.kind]);
    }
    if (tok.kind == TOK_ERR) break;
    if (tok.kind == TOK_EOF) break;
    lex_eat_token(&l);
  }
}

int main(void)
{
  String8 input = input_zero;
  Arena* arena = arena_alloc();
  double result = eval_string(arena, input);
  printf("[%.*s] = %lf\n", Str8Fmt(input), result);
  arena_release(arena);
}



#include "arena.c"
