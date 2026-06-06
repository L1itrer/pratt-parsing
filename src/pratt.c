#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "arena.h"

typedef uint64_t u64;
typedef int64_t  i64;
typedef int32_t  i32;

#define ArrSize(arr) ((sizeof(arr)/sizeof(arr[0])))

#define OPERATORS \
X(TOK_ERR, 0)\
X(TOK_NUM, 0)\
X(TOK_PLUS, 2)\
X(TOK_MINUS, 2)\
X(TOK_ASTERISK, 3)\
X(TOK_FORWARD_SLASH, 3)\
X(TOK_EOF, 0)\

typedef enum TokenKind {
#define X(tok, prec) tok,
OPERATORS
#undef X
} TokenKind;

const i32 tok_prec_table[] = {
#define X(tok, prec) prec,
OPERATORS
#undef X
};

typedef struct Token {
  u64 kind;
  double value;
}Token;


typedef enum {
  ERR_NONE = 0,
  ERR_UNEXPECTED_TOKEN,
} ParseError;

Token input_right[] = {
  {TOK_NUM, 1.0},
  {TOK_MINUS, 0.0},
  {TOK_NUM, 2.0},
  {TOK_PLUS, 0.0},
  {TOK_NUM, 3.0},
  {TOK_ASTERISK, 0.0},
  {TOK_NUM, 4.0},
  {TOK_EOF, 0.0},
};


Token input_left[] = {
  {TOK_NUM, 4.0},
  {TOK_ASTERISK, 0.0},
  {TOK_NUM, 3.0},
  {TOK_PLUS, 0.0},
  {TOK_NUM, 2.0},
  {TOK_MINUS, 0.0},
  {TOK_NUM, 1.0},
  {TOK_EOF, 0.0},
};

typedef struct AstNode AstNode;
struct AstNode {
  i64 kind;
  AstNode* left;
  AstNode* right;
  double value;
};

typedef struct Lexer {
  Token* stream;
  i64 pos;
} Lexer;

Lexer lexer_init(Token* in)
{
  return (Lexer){.pos = 0, .stream = in};
}

Token lex_peek_token(Lexer* l)
{
  return l->stream[l->pos];
}


Token lex_get_token(Lexer* l)
{
  Token tok = l->stream[l->pos];
  l->pos += 1;
  return tok;
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
  return (tok.kind > TOK_NUM) && (tok.kind < TOK_EOF);
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


AstNode* parse_expression_right(Arena* a, Lexer* l, ParseError* err)
{
  (void)err;
  Token tok = {0};
  AstNode* root = NULL;
  AstStack* s = arena_push_struct(a, AstStack);

  AstNode* op;
  AstStackNode* snode;

  tok = lex_get_token(l);
  if (tok.kind != TOK_NUM) return NULL; // todo: report error

  root = make_number(a, tok.value);

  while (1)
  {
    tok = lex_get_token(l);

    if (is_binary_operator(tok))
    {
      // push on the stack
      op = make_operator(a, tok.kind);
      op->left = root;
      snode = arena_push_struct(a, AstStackNode);
      snode->node = op;
      StackPush(s, snode);
      tok = lex_get_token(l);
      if (tok.kind != TOK_NUM)
      {
        printf("Unexpected token: %llu\n", tok.kind);
        break;
      }
      root = make_number(a, tok.value);
    }
    else if (tok.kind == TOK_EOF)
    {
      while (!StackIsEmpty(s))
      {
        s->top->node->right = root;
        root = s->top->node;
        s->top = s->top->prev; // pop
      }
      break;
    }
    else
    {
      printf("Unexpected token: %llu\n", tok.kind);
      return NULL;
    }
  }
  return root;
}


AstNode* parse_expression_left(Arena* a, Lexer* l, ParseError* err)
{
  (void)err;
  AstNode* root = NULL;
  Token tok;
  tok = lex_get_token(l);
  if (tok.kind != TOK_NUM) return NULL;
  root = make_number(a, tok.value);

  AstNode* op;
  while (1)
  {
    tok = lex_get_token(l);
    if (is_binary_operator(tok))
    {
      op = make_operator(a, tok.kind);
      op->left = root;

      tok = lex_get_token(l);
      if (tok.kind != TOK_NUM) return NULL;
      op->right = make_number(a, tok.value);

      root = op;
    }
    else if (tok.kind == TOK_EOF)
    {
      break;
    }
    else
    {
      root = NULL;
      printf("Unexpected token %llu\n", tok.kind);
      break;
    }
  }
  return root;
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


parse_expression_iter(min_prec)
{
  left = parse_leaf()
  while 1
  {
    next = get_next_token()
    if is_binary_operator(next) node = left

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
  Token tok = lex_get_token(l);
  AssertAlways(tok.kind == TOK_NUM);
  return make_number(a, tok.value);
}

AstNode* parse_expression_rec(Arena* a, Lexer* l, ParseError* err, i32 min_prec)
{
  AstNode* root = NULL;
  Token tok = lex_get_token(l);
  AssertAlways(tok.kind == TOK_NUM);

  AstNode* left = make_number(a, tok.value);

  for (;;)
  {
    tok = lex_peek_token(l);
    if (tok.kind == TOK_NUM)
    {
      root = left;
      lex_get_token(l);
      break;
    }
    else if (is_binary_operator(tok))
    {
      i32 next_prec = tok_prec_table[tok.kind];

      if (next_prec > min_prec)
      {
        lex_get_token(l);
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
      Trap();
    }
  }

  return root;
}

AstNode* parse_expression(Arena* a, Lexer* l, ParseError* err)
{
  return parse_expression_rec(a, l, err, -9999);
}

int main(void)
{
  Lexer l = lexer_init(input_right);
  Arena* arena = arena_alloc();
  ParseError err = ERR_NONE;
  AstNode* res = parse_expression(arena, &l, &err);
  if (err == ERR_NONE)
  {
    printf("OK\n");
  }
  printf("%p\n", res);
  arena_release(arena);
}



#include "arena.c"
