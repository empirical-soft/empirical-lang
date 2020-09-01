/*
 * VVM Assembly Syntax
 *
 * The grammar here defines VVM's assembly language. ANTLR converts this file
 * into an abstract Visitor class that represents the resulting parse tree.
 */

grammar VVMAsm;

prog : ((instruction | defvalue | deftype)? EOL)+;

instruction : IDENTIFIER (operand* | ':');
defvalue    : REGISTER '=' (value | funcdef);
deftype     : UDT '=' newtype;

operand : type | INT | REGISTER;

value : INT   # IntValue
      | FLOAT # FloatValue
      | STR   # StrValue
      ;

funcdef : DEF name=IDENTIFIER '(' typelist? ')' type ':' prog END;

newtype  : '{' typelist '}';
typelist : ntype (',' ntype)*;
ntype    : (name=STR ':')? type;
type     : IDENTIFIER | UDT;

INT   : [0-9]+;
FLOAT : INT '.' INT;
STR   : '"' ~'"'+ '"';

DEF : 'def';
END : 'end';

IDENTIFIER : [a-zA-Z] [a-zA-Z0-9_]*;
REGISTER   : [%@*] INT;
UDT        : '$' INT;

EOL     : [\r\n]+;
WS      : [ \t] -> skip;
COMMENT : ';' ~[\r\n]* -> skip;

