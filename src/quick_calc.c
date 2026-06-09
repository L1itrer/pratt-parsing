#include <stdint.h>
#ifdef __clang__
#include "arena.h"
#include "fred_api.h"
#endif
#define QC_OPERATORS \
X(QC_TOK_ERR, 0, "[ERROR]")\
X(QC_TOK_NUM, 0, "num")\
X(QC_TOK_PLUS, 2, "+")\
X(QC_TOK_MINUS, 2, "-")\
X(QC_TOK_ASTERISK, 3, "*")\
X(QC_TOK_FORWARD_SLASH, 3, "/")\
X(QC_TOK_OPEN_PAREN, 0, "(")\
X(QC_TOK_CLOSE_PAREN, 0, ")")\
X(QC_TOK_EOF, 0, "eof")\

#define push_struct(a, T) push_array(a, T, 1)

typedef enum QCTokenKind {
#define X(tok, prec, print) tok,
QC_OPERATORS
#undef X
} QCTokenKind;

const int32_t qc_tok_prec_table[] = {
#define X(tok, prec, print) prec,
QC_OPERATORS
#undef X
};

const char* qc_tok_printable[] = {
#define X(tok, prec, print) print,
QC_OPERATORS
#undef X
};

typedef struct QCToken {
  int64_t kind;
  int64_t pos;
  double value;
}QCToken;

#define QC_ERRORS \
X(QC_ERR_NONE, "QC_ERR_NONE") \
X(QC_ERR_UNEXPECTED_TOKEN, "QC_ERR_UNEXPECTED_TOKEN") \
X(QC_ERR_MISMATCHED_PARENS, "QC_ERR_MISMATCHED_PARENS") \
X(QC_ERR_UNKNOWN, "QC_ERR_UNKNOWN") \


typedef enum {
#define X(err, str) err, 
QC_ERRORS
#undef X
  //QC_ERR_NONE = 0,
  //QC_ERR_UNEXPECTED_TOKEN,
  //QC_ERR_MISMATCHED_PARENS,
  //QC_ERR_DIVIDE_BY_ZERO,
} QCErrorEnum;

const char* qc_err_strs[] = {
#define X(err, str) str, 
QC_ERRORS
#undef X
};

int qc_is_binary_operator(QCToken tok);


typedef struct QCResult {
  QCErrorEnum status;
  double value;
}QCResult;

typedef struct QCAstNode QCAstNode;
struct QCAstNode {
  int64_t kind;
  QCAstNode* left;
  QCAstNode* right;
  double value;
};

typedef struct{
  QCToken tok;
  QCToken prev_tok;
  String8 stream;
  int64_t pos;
}QCLexer;

QCLexer qc_lexer_init(String8 in)
{
  return (QCLexer){
    .pos = 0,
    .stream = in,
    .tok = (QCToken){.kind = -1, .pos = -1},
    .prev_tok = {}
  };
}

char qc_lex_peek_char(QCLexer* l)
{
  //Assert(l->pos <= l->stream.len);
  if (l->pos >= (int64_t)l->stream.size) return -1;
  return l->stream.str[l->pos];
}

void qc_lex_eat_char(QCLexer* l)
{
  l->pos += 1;
  if (l->pos > (int64_t)l->stream.size) l->pos = l->stream.size;
  //Assert(l->pos <= l->stream.len);
}

int qc_is_whitespace(char c)
{
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void qc_skip_whitespace(QCLexer* l)
{
  for (;;)
  {
    char c = qc_lex_peek_char(l);
    if (qc_is_whitespace(c)) qc_lex_eat_char(l);
    else break;
  }
}

int qc_is_num(char c)
{
  return (c >= '0' && c <= '9');
}

int qc_is_binary_operator(QCToken tok)
{
  return (tok.kind > QC_TOK_NUM) && (tok.kind < QC_TOK_OPEN_PAREN);
}



String8 qc_scan_number(QCLexer* l)
{
  String8 result = l->stream;
  result.size = 0;
  result.str = l->stream.str + l->pos;
  for (;;)
  {
    char c = qc_lex_peek_char(l);
    if (qc_is_num(c) || c == '.' || (c == '-' && result.size == 0))
    {
      qc_lex_eat_char(l);
      result.size += 1;
    }
    else break;
  }
  return result;
}

int qc_try_parse_num(String8 str, double* num)
{
  int result = 1;
  if (!try_f64_from_str8(str, num))
  {
    result = 0;
    *num = 0.0;
  }

  return result;
}

QCToken qc_lex_peek_token(QCLexer* l)
{
  if (l->tok.kind != -1) return l->tok;
  QCToken* tok = &l->tok;
  *tok = (QCToken){.kind = QC_TOK_ERR, .pos = l->pos};
  char c = -1;
  qc_skip_whitespace(l);
  c = qc_lex_peek_char(l);
  switch (c)
  {
    case -1:
      {
        tok->kind = QC_TOK_EOF;
      } break;
    case '+':
      {
        tok->kind = QC_TOK_PLUS;
        qc_lex_eat_char(l);
      } break;
    case '-':
      {
        qc_lex_eat_char(l);
        c = qc_lex_peek_char(l);
        if (qc_is_num(c))
        {
          // if the previous token was an operator it's a unary minus
          // @Hack: and also it it's the start of input it's also a unary minus
          if (qc_is_binary_operator(l->prev_tok) || l->pos == 1)
          {
            l->pos -= 1;
            goto number;
          }
        }
        tok->kind = QC_TOK_MINUS;
      } break;
    case '*':
      {
        tok->kind = QC_TOK_ASTERISK;
        qc_lex_eat_char(l);
      } break;
    case '/':
      {
        tok->kind = QC_TOK_FORWARD_SLASH;
        qc_lex_eat_char(l);
      } break;
    case '(':
      {
        tok->kind = QC_TOK_OPEN_PAREN;
        qc_lex_eat_char(l);
      } break;
    case ')':
      {
        tok->kind = QC_TOK_CLOSE_PAREN;
        qc_lex_eat_char(l);
      } break;
    default:
      {
number:
        if (qc_is_num(c))
        {
          String8 num_str = qc_scan_number(l);
          if (num_str.str == NULL) goto err;
          double num = 0.0;
          if (!qc_try_parse_num(num_str, &num))
          {
            tok->kind = QC_TOK_ERR;
          }
          else
          {
            tok->kind = QC_TOK_NUM;
            tok->value = num;
          }
        }
        else
        {
err:
          tok->kind = QC_TOK_ERR;
        }
      } break;
  }
  return l->tok;
}

void qc_lex_eat_token(QCLexer* l)
{
  l->prev_tok = l->tok;
  l->tok = (QCToken){.kind = -1};
}

QCAstNode* qc_make_operator(Arena* a, QCTokenKind kind)
{
  QCAstNode* node = push_struct(a, QCAstNode);
  node->kind = kind;
  node->value = 0.0;
  return node;
}

QCAstNode* qc_make_number(Arena* a, double val)
{
  QCAstNode* node = push_struct(a, QCAstNode);
  node->kind = QC_TOK_NUM;
  node->value = val;
  return node;
}

QCAstNode* qc_parse_expression_recursive(Arena* a, QCLexer* l, QCErrorEnum* err, int32_t min_prec)
{
  QCAstNode* root = NULL;
  QCAstNode* left = NULL;
  QCToken tok = {0};

  for (;;)
  {
    if (*err != QC_ERR_NONE) break;
    tok = qc_lex_peek_token(l);
    if (tok.kind == QC_TOK_NUM)
    {
      left = qc_make_number(a, tok.value);
      root = left;
      qc_lex_eat_token(l);
      // break;
    }
    else if (tok.kind == QC_TOK_OPEN_PAREN)
    {
      qc_lex_eat_token(l);
      left = qc_parse_expression_recursive(a, l, err, -999);
      root = left;
      tok = qc_lex_peek_token(l);
      if (tok.kind != QC_TOK_CLOSE_PAREN)
      {
        *err = QC_ERR_MISMATCHED_PARENS;
        break;
      }
      qc_lex_eat_token(l);
    }
    else if (tok.kind == QC_TOK_CLOSE_PAREN)
    {
      break;
    }
    else if (qc_is_binary_operator(tok))
    {
      int32_t next_prec = qc_tok_prec_table[tok.kind];

      if (next_prec > min_prec)
      {
        qc_lex_eat_token(l);
        root = qc_make_operator(a, tok.kind);
        root->left = left;
        root->right = qc_parse_expression_recursive(a, l, err, next_prec);
      }
      else
      {
        root = left;
        break;
      }
      left = root;
    }
    else if (tok.kind == QC_TOK_EOF)
    {
      root = left;
      break;
    }
    else
    {
      *err = QC_ERR_UNEXPECTED_TOKEN;
    }
  }

  return root;
}

QCAstNode* qc_parse_expression(Arena* a, QCLexer* l, QCErrorEnum* err)
{
  return qc_parse_expression_recursive(a, l, err, -9999);
}

double qc_eval_expression_recursive(QCAstNode* root)
{
  if (root->kind == QC_TOK_NUM) return root->value;
  double lhs = 12345.0, rhs = 54321.0;
  if (root->left != NULL) lhs = qc_eval_expression_recursive(root->left);
  if (root->right != NULL) rhs = qc_eval_expression_recursive(root->right);
  if (root->kind == QC_TOK_PLUS) return lhs + rhs;
  if (root->kind == QC_TOK_MINUS) return lhs - rhs;
  if (root->kind == QC_TOK_ASTERISK) return lhs * rhs;
  if (root->kind == QC_TOK_FORWARD_SLASH) return lhs / rhs; // it's not invalid to divide by  floating zero but still not sure if its correct
  //Assert(0); // should be unreachable
  return 2137.0;
}

typedef struct QCStackNode QCStackNode;
struct QCStackNode {
  QCAstNode* node;
  QCStackNode* prev;
  double* parent_value;
  double subtree_result_left;
  double subtree_result_right;
  uint32_t visited;
};

typedef struct {
  QCStackNode* top;
} QCStack;



#define QCStackIsEmpty(s) (s->top == NULL)
#define QCStackPeek(s) (s->top)
#define QCStackPop(s) do{\
    s->top = s->top->prev;\
}while(0)
#define QCStackPush(s, n) do{ \
  n->prev = s->top; \
  s->top = n; \
 }while(0)

QCStackNode* qc_make_stack_node(Arena* a, QCAstNode* root, double* parent_value)
{
  QCStackNode* res = push_struct(a, QCStackNode);
  res->node = root;
  res->parent_value = parent_value;
  return res;
}

uint32_t qc_eval_expression_iterative(Arena* a, QCAstNode* root, double* num)
{
  uint32_t result = 1;
  QCStack stack_static = {0};
  QCStack* s = &stack_static; // to make dereference a '->' not a '.'
  s->top = qc_make_stack_node(a, root, num);
  while (!QCStackIsEmpty(s))
  {
    QCStackNode* top = QCStackPeek(s);
    QCAstNode* curr_node = top->node;
    if (curr_node->kind == QC_TOK_NUM)
    {
      *top->parent_value = curr_node->value;
      QCStackPop(s);
    }
    else
    {
      if (!top->visited)
      {
        top->visited = 1;
        if (!curr_node->left || !curr_node->right)
        {
          result = 0; //something went wrong while parsing and we did not catch that
          break;
        }
        QCStackNode* right = qc_make_stack_node(a, curr_node->right, &top->subtree_result_right);
        QCStackNode* left  = qc_make_stack_node(a, curr_node->left,  &top->subtree_result_left);
        QCStackPush(s, right);
        QCStackPush(s, left);
      }
      else
      {
        double lhs = top->subtree_result_left;
        double rhs = top->subtree_result_right;
        double op_result;

        if (curr_node->kind == QC_TOK_PLUS) op_result = lhs + rhs;
        else if (curr_node->kind == QC_TOK_MINUS) op_result =  lhs - rhs;
        else if (curr_node->kind == QC_TOK_ASTERISK) op_result = lhs * rhs;
        else if (curr_node->kind == QC_TOK_FORWARD_SLASH) op_result = lhs / rhs; // @Verify is divide by zero something to check
        else
        {
          result = 0; //something went wrong while parsing and we did not catch that
          break;
        }
        *top->parent_value = op_result;
        QCStackPop(s);
      }
    }
  }

  return result;
}


void test_lex_string(Arena* a, String8 input)
{
  QCLexer l = qc_lexer_init(input);
  for (;;)
  {
    QCToken tok = qc_lex_peek_token(&l);
    if (tok.kind == QC_TOK_NUM)
    {
      feed_queue_info(str8_fmt(a, "num = %lf", tok.value));
    }
    else
    {
      feed_queue_info(str8_fmt(a, "%d", tok.kind));
    }
    if (tok.kind == QC_TOK_ERR) break;
    if (tok.kind == QC_TOK_EOF) break;
    qc_lex_eat_token(&l);
  }
}

QCResult qc_core(Arena* arena, String8 input) {
    QCResult result = {0};
    if (input.size > 0)
    {
      QCLexer l = qc_lexer_init(input);
      QCAstNode* parse_result = qc_parse_expression(arena, &l, &result.status);
      //feed_queue_info(str8_fmt(arena, "parse_result %d", result.status));
      if (result.status == QC_ERR_NONE)
      {
        //result.value = qc_eval_expression_recursive(parse_result);
        uint32_t evaled = qc_eval_expression_iterative(arena, parse_result, &result.value);
        if (!evaled)
        {
          feed_queue_error((String8)str8_lit("Something went really wrong when evaluating! Sorry! Go harass me on discord"));
          result.status = QC_ERR_UNKNOWN;
        }
      }
      else
      {
        String8 msg;
        switch (result.status)
        {
          case QC_ERR_UNEXPECTED_TOKEN:
          {
            msg = str8_fmt(arena, "Parse error: unexpected token at offset %d", l.tok.pos);
          } break;
          case QC_ERR_MISMATCHED_PARENS:
          {
            msg = str8_fmt(arena, "Parse error: mismatched paren at offset %d", l.tok.pos);
          } break;
          default:
          {
            msg = str8_fmt(arena, "Parse error: something really weird at offset %d", l.tok.pos);
          } break; 
        }
        feed_queue_error(msg);
      }
    }

    return result;
}

#ifndef FRED_EMULATION
#ifndef __clang__

DEF_PLUGIN_EDITOR_HOOK("Quick calc (msg)", "Calculates the selected text and sends the result to messages", quick_calc_msg) {
    Temp scratch = scratch_begin(NULL);
    String8Array strings;
    ed_cursor_selections(scratch.arena, ctx, &strings);
    String8 msg = {0};
    if (strings.size == 1) {
      #if 0
      test_lex_string(scratch.arena, strings.strs[0]);
      #else
      QCResult result = qc_core(scratch.arena, strings.strs[0]);
      if (result.status == QC_ERR_NONE) {
        msg = str8_fmt(scratch.arena, "%f", result.value);
        feed_queue_info(msg);
      }
      else {

      }
      #endif
    }
    else {
      for EachIndex(i, strings.size) {
      String8 str = strings.strs[i];
      if (str.size != 0) {
          QCResult result = qc_core(scratch.arena, str);
          msg = str8_fmt(scratch.arena, "%d. [%S] = %f", i, str, result.value);
      } else {
          msg = str8_fmt(scratch.arena, "%d. (empty)", i);
      }
      feed_queue_info(msg);
    }
  }

  scratch_end(scratch);
}


DEF_PLUGIN_EDITOR_HOOK("Quick calc (replace)", "Calculates the selected text and replaces with the result", quick_calc_replace) {
  Temp scratch = scratch_begin(NULL);
  String8Array strings;
  ed_cursor_selections(scratch.arena, ctx, &strings);
  String8Array ins_buf = {0};
  ins_buf.size = strings.size;
  ins_buf.strs = push_array(scratch.arena, String8, strings.size);
  String8 msg = {0};
  uint32_t were_errors = 0;
  for EachIndex(i, strings.size) {
    String8 str = strings.strs[i];
    if (str.size != 0) {
      QCResult result = qc_core(scratch.arena, str);
      if (result.status == QC_ERR_NONE) {
        msg = str8_fmt(scratch.arena, "%f", str, result.value);
        ins_buf.strs[i].str = push_array_no_zero(scratch.arena, char, msg.size);
        ins_buf.strs[i].size = msg.size;
        memcpy(ins_buf.strs[i].str, msg.str, msg.size);
      }
      else {
        were_errors = 1;
      }
    }
    //feed_queue_info(msg);
  }
  if (!were_errors) {
    EditorCmd cmd = {0};
    cmd.cmd = ED_MCInsertGroup;
    cmd.buffers = ins_buf;
    ed_push_command(ctx, &cmd);
  }
  scratch_end(scratch);
}

#endif // __clang__
#endif // FRED_EMULATION
