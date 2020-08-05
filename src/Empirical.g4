/*
 * Concrete Syntax
 *
 * The grammar here defines the Empirical syntax. ANTLR converts this file
 * into an abstract Visitor class that represents the resulting parse tree.
 */

grammar Empirical;

@parser::members {
  bool nlAhead() {
    int possibleIndexEosToken = getCurrentToken()->getTokenIndex() - 1;
    antlr4::Token* ahead = _input->get(possibleIndexEosToken);
    return ahead->getType() == EmpiricalParser::NEWLINE;
  }

  bool endAhead() {
    int next = _input->LT(1)->getType();
    return (next == EmpiricalParser::END ||
            next == EmpiricalParser::ELIF ||
            next == EmpiricalParser::ELSE);
  }

  /* Java -- useful for grun
  boolean nlAhead() {
    int possibleIndexEosToken = this.getCurrentToken().getTokenIndex() - 1;
    Token ahead = _input.get(possibleIndexEosToken);

    return (ahead.getChannel() == Lexer.HIDDEN) &&
           (ahead.getType() == EmpiricalParser.NEWLINE);
  }

  boolean endAhead() {
    int next = _input.LT(1).getType();
    return (next == EmpiricalParser.END ||
            next == EmpiricalParser.ELIF ||
            next == EmpiricalParser.ELSE);
  }
  */
}


/* statements */
input : stmt* EOF;

stmt : simple_stmt
     | compound_stmt
     ;

simple_stmt : small_stmt (';' small_stmt)* eos;

small_stmt : expr_stmt
           | decl_stmt
           | del_stmt
           | import_stmt
           | return_stmt
           ;

compound_stmt : funcdef
              | datadef
              | if_stmt
              | while_stmt
              ;

suite : simple_stmt
      | stmt+
      ;

eos : ';' | EOF | {endAhead()}? | {nlAhead()}?;


/* function definition */
funcdef     : FUNC name=func_name ('{' templates=decl_list '}')?
              '(' args=decl_list? ')' (ARROW rettype=expr)? ':'
              body=suite END;
func_name   : NAME | oper;
decl_list   : declaration (',' declaration)*;
declaration : name=NAME (':' type=expr)? ('=' value=expr)?;


/* data definition */
datadef : DATA name=NAME ':' body=decl_list END;


/* control flow */
if_stmt    : IF test=expr ':' body=suite (ELIF expr ':' suite)*
             (ELSE ':' else_body=suite)? END;
while_stmt : WHILE test=expr ':' body=suite END;


/* delete statement */
del_stmt  : DEL target=expr_list;
expr_list : expr (',' expr)*;


/* import statement */
import_stmt : import_name
            | import_from
            ;

import_name     : IMPORT names=dotted_as_names;
dotted_as_names : dotted_as_name (',' dotted_as_name)*;
dotted_as_name  : name=dotted_name (AS asname=NAME)?;

import_from     : FROM module=dotted_name IMPORT ('*' | names=import_as_names);
import_as_names : import_as_name (',' import_as_name)*;
import_as_name  : name=NAME (AS asname=NAME)?;

dotted_name : NAME ('.' NAME)*;


/* flow statements */
return_stmt : RETURN expr?;


/* declarations */
decl_stmt : dt=(LET|VAR) decls=decl_list;


/* expressions */
nexpr_list : nexpr (',' nexpr)*;
nexpr      : (name=NAME '=')? value=expr;

expr_stmt : expr ('=' expr)?;

direction : dt=(BACKWARD | FORWARD | NEAREST);

join_params : ON on=nexpr_list
            | ASOF asof=nexpr_list
            | STRICT_
            | dt=direction
            | WITHIN within=expr
            ;

expr : FROM table=expr qt=(SELECT|EXEC) (cols=nexpr_list)?
       (BY by=nexpr_list)? (WHERE where=expr)?                # QueryExpr
     | SORT table=expr BY by=nexpr_list                       # SortExpr
     | JOIN left=expr ',' right=expr param=join_params*       # JoinExpr
     | op=('+'|'-') operand=expr                              # UnOpExpr
     | left=expr op=('*'|'/'|'%') right=expr                  # BinOpExpr
     | left=expr op=('+'|'-') right=expr                      # BinOpExpr
     | left=expr op=('<<'|'>>') right=expr                    # BinOpExpr
     | left=expr op='&' right=expr                            # BinOpExpr
     | left=expr op='|' right=expr                            # BinOpExpr
     | left=expr op=('<'|'>'|'=='|'<='|'>='|'!=') right=expr  # BinOpExpr
     | op=NOT operand=expr                                    # UnOpExpr
     | left=expr op=OR right=expr                             # BinOpExpr
     | left=expr op=AND right=expr                            # BinOpExpr
     | value=atom ('{' templates=arg_list? '}')?
       right=trailer*                                         # AtomExpr
     | '(' expr ')'                                           # ParenExpr
     ;


/* atoms */
atom : NAME       # NameAtom
     | oper       # OperAtom
     | list       # ListAtom
     | number     # NumAtom
     | string+    # StrAtom
     | character  # CharAtom
     | TRUE       # TrueAtom
     | FALSE      # FalseAtom
     | NIL        # NilAtom
     | NaN        # NanAtom
     ;


/* trailers */
trailer : '$'? '(' arg_list? ')'
        | '[' subscript ']'
        | '.' NAME;


/* function arguments */
arg_list : argument (',' argument)*;
argument : expr                        # PositionalArgExpr
         | name=NAME '=' value=expr    # KeywordArgExpr
         ;


/* array subscript */
subscript : expr                                        # SimpleSubscriptExpr
          | lower=expr? ':' upper=expr? step=sliceop?   # SliceExpr
          ;
sliceop   : ':' expr?;


/* reserved words and symbols */
FROM   : 'from';
SELECT : 'select';
EXEC   : 'exec';
BY     : 'by';
WHERE  : 'where';
SORT   : 'sort';

JOIN : 'join';
ON   : 'on';
ASOF : 'asof';

STRICT_  : 'strict';      // "STRICT" conflicts with a Windows macro
BACKWARD : 'backward';
FORWARD  : 'forward';
NEAREST  : 'nearest';
WITHIN   : 'within';

LET : 'let';
VAR : 'var';

IMPORT : 'import';
AS     : 'as';
DEL    : 'del';

DATA   : 'data';
FUNC   : 'func';

IF    : 'if';
ELIF  : 'elif';
ELSE  : 'else';
WHILE : 'while';
END   : 'end';

RETURN : 'return';

NOT : 'not';
OR  : 'or';
AND : 'and';

TRUE  : 'true';
FALSE : 'false';
NIL   : 'nil';
NaN   : 'nan';

ADD    : '+';
SUB    : '-';
MUL    : '*';
DIV    : '/';
LSHIFT : '<<';
RSHIFT : '>>';
BITAND : '&';
BITOR  : '|';

LT    : '<';
GT    : '>';
EQ    : '==';
LTE   : '<=';
GTE   : '>=';
NOTEQ : '!=';

ARROW : '->';


/* operators */
oper : '(' op=('+'|'-'|'*'|'/'|'%'|'<<'|'>>'|'&'|'|'|
               AND|OR|NOT|
               '<'|'>'|'=='|'<='|'>='|'!=') ')';


/* lists */
list : '[' expr (',' expr)* ']';


/* numbers */
number  : integer       # IntNumber
        | FLOAT_NUMBER  # FloatNumber
        ;

integer : OCT_INTEGER  # OctInt
        | HEX_INTEGER  # HexInt
        | BIN_INTEGER  # BinInt
        | DEC_INTEGER  # DecInt
        ;

FLOAT_NUMBER : EXPONENT_FLOAT NAME? | POINT_FLOAT NAME?;

OCT_INTEGER : '0' [oO] OCT_DIGIT+ NAME?;
HEX_INTEGER : '0' [xX] HEX_DIGIT+ NAME?;
BIN_INTEGER : '0' [bB] BIN_DIGIT+ NAME?;
DEC_INTEGER : NON_ZERO_DIGIT DIGIT* NAME? | '0'+ NAME?;

fragment NON_ZERO_DIGIT : [1-9];
fragment DIGIT          : [0-9];
fragment OCT_DIGIT      : [0-7];
fragment HEX_DIGIT      : [0-9a-fA-F];
fragment BIN_DIGIT      : [01];

fragment POINT_FLOAT    : INT_PART? FRACTION | INT_PART '.';
fragment EXPONENT_FLOAT : (INT_PART | POINT_FLOAT) EXPONENT;

fragment INT_PART : DIGIT+;
fragment FRACTION : '.' DIGIT+;
fragment EXPONENT : [eE] [+-]? DIGIT+;


/* strings */
string : STRING_LITERAL;

STRING_LITERAL : SHORT_STRING | LONG_STRING;

fragment SHORT_STRING : '"' SHORT_STRING_ITEM* '"';
fragment LONG_STRING  : '"""' LONG_STRING_ITEM* '"""';

fragment SHORT_STRING_ITEM : STRING_ESCAPE_SEQ | SHORT_STRING_CHAR;
fragment LONG_STRING_ITEM  : STRING_ESCAPE_SEQ | LONG_STRING_CHAR;

fragment STRING_ESCAPE_SEQ : '\\' .;
fragment SHORT_STRING_CHAR : ~[\\\r\n\f"];
fragment LONG_STRING_CHAR  : ~'\\';


/* characters */
character : CHAR_LITERAL;

CHAR_LITERAL : '\'' CHAR_ITEM* '\'';

fragment CHAR_ITEM : STRING_ESCAPE_SEQ | CHAR;
fragment CHAR      : ~[\\\r\n\f'];


/* identifiers */
NAME : '!'? ID_START ID_CONTINUE*;

fragment ID_START    : '_' | [a-zA-Z];
fragment ID_CONTINUE : ID_START | [0-9];


/* ignore */
SKIP_ : (SPACES | COMMENT) -> skip;

fragment SPACES  : [ \t]+;
fragment COMMENT : '#' ~[\r\n\f]*;

NEWLINE : ('\r'? '\n'|'\r'|'\f') -> channel(HIDDEN);

