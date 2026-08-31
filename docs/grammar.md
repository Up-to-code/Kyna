# Grammar (implemented subset)

Notation: `*` zero or more, `?` optional.

```ebnf
program       ::= (import | import-list | import-default | import-namespace)* declaration* EOF ;
import        ::= "import" STRING "as" IDENT ";" ;
import-list   ::= "import" "{" import-specifier ("," import-specifier)* "}" "from" STRING ";" ;
import-specifier ::= IDENT ("as" IDENT)? ;
import-default   ::= "import" IDENT "from" STRING ";" ;
import-namespace ::= "import" "*" "as" IDENT "from" STRING ";" ;
declaration   ::= "export"? modifiers* ( variable | function | class | interface ) | statement ;
export-list   ::= "export" "{" IDENT ("," IDENT)* "}" ";" ;
export-default-var ::= "export" "default" (variable | function | class | interface) ;
variable      ::= ("let" | "set") IDENT (":" type)? ("=" expression)? ";" ;
function      ::= "func" IDENT "(" parameters? ")" (":" type)? block ;
parameters    ::= IDENT ":" type ("," IDENT ":" type)* ;
class         ::= "class" IDENT ("extends" IDENT)? ("implements" type-arg ("," type-arg)*)? "{" class-member* "}" ;
class-member  ::= modifiers* ("init" | "func" IDENT) "(" parameters? ")" (":" type)? (block | ";")
                | modifiers* IDENT ":" type ("=" expression)? ";" ;
interface     ::= "intf" IDENT ("<" type-param ("," type-param)* ">")?
                  ("extends" type-arg ("," type-arg)*)? "{" interface-member* "}" ;
interface-member ::= "func" IDENT "(" parameters? ")" ":" type ";"
                   | IDENT ":" type ";" | IDENT "?" ":" type ";"
                   | "[" IDENT ":" type "]" ":" type ";"        (* index signature *)
                   | "(" parameters? ")" ":" type ";"          (* call signature *)
                   | IDENT "(" parameters? ")" ":" type ";" ;  (* method *)
type-param    ::= IDENT ;
type          ::= IDENT ("<" type ("," type)* ">")? ("?")? ("|" type)* ;
```

Precedence from low to high is assignment, `||`, `&&`, equality, comparison, `+/-`, `*/%`, unary, call/member.

Source files may use the `.kyna`/`.ky` extension for programs and `.kyna.d`/`.d.ky`/`.ky.d` for ambient type-definition files.
