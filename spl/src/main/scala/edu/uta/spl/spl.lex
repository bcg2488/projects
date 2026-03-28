/********************************************************************************
*
* File: spl.lex
* The SPL scanner
*
********************************************************************************/

package edu.uta.spl;

import java_cup.runtime.Symbol;

%%
%class SplLex
%public
%line
%column
%cup

DIGIT=[0-9]*
ID=[a-zA-Z][a-zA-Z0-9_]*

%{

  private Symbol symbol ( int type ) {
    return new Symbol(type, yyline+1, yycolumn+1);
  }

  private Symbol symbol ( int type, Object value ) {
    return new Symbol(type, yyline+1, yycolumn+1, value);
  }

  public void lexical_error ( String message ) {
    System.err.println("*** Lexical Error: " + message + " (line: " + (yyline+1)
                       + ", position: " + (yycolumn+1) + ")");
    System.exit(1);
  }
%}

%%

"array"         { return symbol(sym.ARRAY); }
"print"         { return symbol(sym.PRINT); }
"("             { return symbol(sym.LP); }
"int"           { return symbol(sym.INT); }
"var"           { return symbol(sym.VAR); }
")"             { return symbol(sym.RP); }
";"             { return symbol(sym.SEMI); }
"="           { return symbol(sym.EQUAL); }
"["           { return symbol(sym.LSB); }
"]"           { return symbol(sym.RSB); }
{DIGIT}       { return new Symbol(sym.INTEGER_LITERAL,new Integer(yytext())); }
","           { return symbol(sym.COMMA); }
":"           { return symbol(sym.COLON); }
"true"           { return symbol(sym.TRUE); }
"false"           { return symbol(sym.FALSE); }
"-"           { return symbol(sym.MINUS); }
"def"           { return symbol(sym.DEF); }
"{"           { return symbol(sym.LB); }
"}"           { return symbol(sym.RB); }
"while"           { return symbol(sym.WHILE); }
"<"           { return symbol(sym.LT); }
"if"           { return symbol(sym.IF); }
"else"           { return symbol(sym.ELSE); }
"&&"           { return symbol(sym.AND); }
"||"           { return symbol(sym.OR); }
"+"           { return symbol(sym.PLUS); }
"=="           { return symbol(sym.EQ); }
"<="           { return symbol(sym.LEQ); }
"return"           { return symbol(sym.RETURN); }
"*"           { return symbol(sym.TIMES); }
"read"           { return symbol(sym.READ); }
">="           { return symbol(sym.GEQ); }
">"           { return symbol(sym.GT); }
"for"           { return symbol(sym.FOR); }
"to"           { return symbol(sym.TO); }
"by"           { return symbol(sym.BY); }
"type"           { return symbol(sym.TYPE); }
"."           { return symbol(sym.DOT); }
"<>"           { return symbol(sym.NEQ); }

{ID}            { return new Symbol(sym.ID,yytext()); }

\"[^\"]*\"              { return new Symbol(sym.STRING_LITERAL,yytext().substring(1,yytext().length()-1)); }

"/*"[^*]*"*/" { /* Ignore comments */ }

[ \t\r\n\f]             { /* ignore white spaces. */ }

.               { lexical_error("Illegal character"); }
