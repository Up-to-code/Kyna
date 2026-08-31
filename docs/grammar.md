# Grammar (implemented subset)

Notation: `*` zero or more, `?` optional.

```ebnf
program       ::= import* declaration* EOF ;
import        ::= "import" STRING "as" IDENT ";" ;
declaration   ::= "export"? modifiers* ( variable | function | class | interface ) | statement ;
variable      ::= ("let" | "set") IDENT (":" type)? ("=" expression)? ";" ;
function      ::= "func" IDENT "(" parameters? ")" (":" type)? block ;
parameters    ::= IDENT ":" type ("," IDENT ":" type)* ;
class         ::= "class" IDENT ("extends" IDENT)? ("implements" IDENT ("," IDENT)*)? "{" class-member* "}" ;
class-member  ::= modifiers* ("init" | "func" IDENT) "(" parameters? ")" (":" type)? (block | ";")
                | modifiers* IDENT ":" type ("=" expression)? ";" ;
interface     ::= "intf" IDENT "{" interface-member* "}" ;
block         ::= "{" (declaration | statement | expression ";")* expression? "}" ;
statement     ::= block | if-statement | while | loop | break | continue | return | throw | try-statement | expression ";" ;
if-statement  ::= "if" "(" expression ")" block ("else" (if-statement | block))? ;
while         ::= "while" "(" expression ")" block ;
loop          ::= (IDENT ":")? "loop" ("(" variable-or-expression ";" expression? ";" expression? ")")? block ;
break         ::= "break" IDENT? ";" ; continue ::= "continue" IDENT? ";" ;
return        ::= "return" expression? ";" ;
throw         ::= "throw" expression ";" ;
try-statement ::= "try" block (("catch" "(" IDENT ")" block ("finally" block)?) | ("finally" block)) ;
type          ::= IDENT | primitive ("?")? ("|" type)* ;
expression    ::= assignment ;
assignment    ::= logic-or ("=" assignment)? ;
primary       ::= literal | array | IDENT | "self" | "super" | "new" IDENT "(" arguments? ")"
                | "(" expression ")" | object | if-expression | match ;
array         ::= "[" (expression ("," expression)*)? "]" ;
postfix       ::= primary ("(" arguments? ")" | "." IDENT | "[" expression "]")* ;
match         ::= "match" "(" expression ")" "{" (literal | "_") "=>" expression ";"* "}" ;
```

Precedence from low to high is assignment, `||`, `&&`, equality, comparison, `+/-`, `*/%`, unary, call/member.
