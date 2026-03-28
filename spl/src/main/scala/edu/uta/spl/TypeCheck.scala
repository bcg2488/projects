package edu.uta.spl

abstract class TypeChecker {
  var trace_typecheck = false

  /** symbol table to store SPL declarations */
  var st = new SymbolTable

  def expandType ( tp: Type ): Type
  def typecheck ( e: Expr ): Type
  def typecheck ( e: Lvalue ): Type
  def typecheck ( e: Stmt, expected_type: Type )
  def typecheck ( e: Definition )
  def typecheck ( e: Program )
}


class TypeCheck extends TypeChecker {

  /** typechecking error */
  def error ( msg: String ): Type = {
    System.err.println("*** Typechecking Error: "+msg)
    System.err.println("*** Symbol Table: "+st)
    System.exit(1)
    null
  }

  /** if tp is a named type, expand it */
  def expandType ( tp: Type ): Type =
    tp match {
      case NamedType(nm)
        => st.lookup(nm) match {
          case Some(TypeDeclaration(t))
              => expandType(t)
          case _ => error("Undeclared type: "+tp)
        }
      case _ => tp
  }

  /** returns true if the types tp1 and tp2 are equal under structural equivalence */
  def typeEquivalence ( tp1: Type, tp2: Type ): Boolean =
    if (tp1 == tp2 || tp1.isInstanceOf[AnyType] || tp2.isInstanceOf[AnyType])
      true
    else expandType(tp1) match {
      case ArrayType(t1)
        => expandType(tp2) match {
              case ArrayType(t2)
                => typeEquivalence(t1,t2)
              case _ => false
           }
      case RecordType(fs1)
        => expandType(tp2) match {
              case RecordType(fs2)
                => fs1.length == fs2.length &&
                   (fs1 zip fs2).map{ case (Bind(v1,t1),Bind(v2,t2))
                                        => v1==v2 && typeEquivalence(t1,t2) }
                                .reduce(_&&_)
              case _ => false
           }
      case TupleType(ts1)
        => expandType(tp2) match {
              case TupleType(ts2)
                => ts1.length == ts2.length &&
                   (ts1 zip ts2).map{ case (t1,t2) => typeEquivalence(t1,t2) }
                                .reduce(_&&_)
              case _ => false
           }
      case _
        => tp2 match {
             case NamedType(n) => typeEquivalence(tp1,expandType(tp2))
             case _ => false
           }
    }

  /* tracing level */
  var level: Int = -1

  /** trace typechecking */
  def trace[T] ( e: Any, result: => T ): T = {
    if (trace_typecheck) {
       level += 1
       println(" "*(3*level)+"** "+e)
    }
    val res = result
    if (trace_typecheck) {
       print(" "*(3*level))
       if (e.isInstanceOf[Stmt] || e.isInstanceOf[Definition])
          println("->")
       else println("-> "+res)
       level -= 1
    }
    res
  }

  /** typecheck an expression AST */
  def typecheck ( e: Expr ): Type =
    trace(e,e match {
      case BinOpExp(op,l,r)
      => val ltp = typecheck(l)
        val rtp = typecheck(r)
        if (!typeEquivalence(ltp,rtp))
          error("Incompatible types in binary operation: "+e)
        else if (op.equals("and") || op.equals("or"))
          if (typeEquivalence(ltp,BooleanType()))
            ltp
          else error("AND/OR operation can only be applied to booleans: "+e)
        else if (op.equals("eq") || op.equals("neq"))
          BooleanType()
        else if (!typeEquivalence(ltp,IntType()) && !typeEquivalence(ltp,FloatType()))
          error("Binary arithmetic operations can only be applied to integer or real numbers: "+e)
        else if (op.equals("gt") || op.equals("lt") || op.equals("geq") || op.equals("leq"))
          BooleanType()
        else ltp

      /* PUT YOUR CODE HERE */
      case StringConst(_) => StringType()
      case IntConst(_) => IntType()
      case FloatConst(_) => FloatType()
      case BooleanConst(_) => BooleanType()

      case CallExp(f,args) =>
        st.lookup(f) match {
          case Some(FuncDeclaration(t,params, _, _, _)) =>
            if (params.length != args.length)
              error("Wrong number of arguments in function call: " +f)
            else
              for ((Bind(_, pt), arg) <- params zip args)
                if (!typeEquivalence(pt, typecheck(arg)))
                  error("Argument type mismatch in function call: " +f)
          t
        case Some(_) => error(f+ " is not a function")
        case None => error("Undefined function: "+f)
        }

      case LvalExp(lval) =>
        typecheck(lval)

      case ArrayExp(elements) =>
        if (elements.isEmpty)
          error("Cannot inder type from empty array: " + e)
        val firstType = typecheck(elements.head)
        for (el <- elements.tail) {
          if (!typeEquivalence(firstType, typecheck(el)))
            error("Inconsistent element types in array: " + e)
        }
        ArrayType(firstType)

      case ArrayGen(size, value) =>
        val sizeType = typecheck(size)
        if (!typeEquivalence(sizeType, IntType()))
          error("Array size must be an integer: " + e)
        val valueType = typecheck(value)
        ArrayType(valueType)

      case RecordExp(fields) =>
        if (fields.isEmpty)
          error("Cannot infer type from empty record: " + e)
        val fieldTypes = fields.map {
          case Bind(name, expr) => Bind(name, typecheck(expr))
        }
        RecordType(fieldTypes)

      case UnOpExp(op, expr) =>
        val exprType = typecheck(expr)
        op match {
          case "minus" =>
            if (!typeEquivalence(exprType, IntType())) {
              error("Unary minus operation can only be applied to integers: " + expr)
            }
            IntType()

          case "not" =>
            if (!typeEquivalence(exprType, BooleanType())) {
              error("Unary 'not' operation can only be applied to booleans: " + expr)
            }
            BooleanType()

          case _ => error("Unsupported unary operator: " + op)
        }

      case NullExp() => AnyType()

      case _ => throw new Error("Wrong expression: "+e)
    } )

  /** typecheck an Lvalue AST */
  def typecheck ( e: Lvalue ): Type =
    trace(e,e match {
      case Var(name)
      => st.lookup(name) match {
        case Some(VarDeclaration(t,_,_)) => t
        case Some(_) => error(name+" is not a variable")
        case None => error("Undefined variable: "+name)
      }

      /* PUT YOUR CODE HERE */

      case ArrayDeref(arr, index) =>
        val indexType = typecheck(index)
        val arrType = expandType(typecheck(arr))
        arrType match {
          case ArrayType(elemType) =>
            if (!typeEquivalence(indexType, IntType()))
              error("Array index must be of type int: " +index)
            elemType
          case _ =>
            error("Trying to index a non-array type: "+arr)
        }

      case RecordDeref(record, attribute) =>
        val recordType = expandType(typecheck(record))
        recordType match {
          case RecordType(fields) =>
            fields.find(_.name == attribute) match{
              case Some(Bind(_,fieldType)) => fieldType
              case None => error(s"Field '$attribute' does not exist in record: $recordType")
            }
          case _ => error(s"Attempting to access field '$attribute' on non-record type: $recordType")
        }

      case _ => throw new Error("Wrong lvalue: "+e)
    } )

  /** typecheck a statement AST using the expected type of the return value from the current function */
  def typecheck ( e: Stmt, expected_type: Type ) {
    trace(e,e match {
      case AssignSt(d,s)
      => if (!typeEquivalence(typecheck(d),typecheck(s)))
        error("Incompatible types in assignment: "+e)

      /* PUT YOUR CODE HERE */
      case BlockSt(decls,stmts) =>
        st.begin_scope()
        decls.foreach(typecheck)
        stmts.foreach(typecheck(_, expected_type))
        st.end_scope()

      case PrintSt(exprs) =>
        exprs.foreach { e =>
          val et = typecheck(e)
          et match {
            case IntType() | FloatType() | StringType() | BooleanType() =>

            case _ =>
              error("Cannot print expression of type: "+et+" in" +e)
          }
        }
        NoType()

      case CallSt(f, args) =>
        st.lookup(f) match {
          case Some(FuncDeclaration(returnType, params, _, _, _)) =>
            if (params.length != args.length) {
              error("Wrong number of arguments in function call: " + f)
            }
            for ((Bind(_, paramType), arg) <- params zip args) {
              if (!typeEquivalence(paramType, typecheck(arg))) {
                error("Argument type mismatch in function call: " + f)
              }
            }
            NoType()

          case Some(_) =>
            error(f + " is not a function")
          case None =>
            error("Undefined function: " + f)
        }

      case ForSt(variable, initial, step, increment, body) =>
        val initType = typecheck(initial)
        val stepType = typecheck(step)
        val incType = typecheck(increment)

        if (!typeEquivalence(initType, IntType()))
          error(s"For loop initial value must be int: $initial")

        if (!typeEquivalence(stepType, IntType()))
          error(s"For loop step value must be int: $step")

        if (!typeEquivalence(incType, IntType()))
          error(s"For loop increment value must be int: $increment")

        st.begin_scope()
        st.insert(variable, VarDeclaration(IntType(), 0, 0))
        typecheck(body, expected_type)
        st.end_scope()

      case ReadSt(lvals) =>
        lvals.foreach { lval =>
          typecheck(lval) match {
            case IntType() | FloatType() | StringType() | BooleanType() =>

            case _ =>
              error("Cannot read into non-variable or invalid type: " + lval)
          }
        }

      case IfSt(cond, thenStmt, elseStmt) =>
        val condType = typecheck(cond)
        if (!typeEquivalence(condType, BooleanType()))
          error("Condition in if-statement must be boolean: "+cond)
        typecheck(thenStmt, expected_type)
        if (elseStmt != null) {
          typecheck(elseStmt, expected_type)
        }

      case WhileSt(cond, body) =>
        val condType = typecheck(cond)
        if (!typeEquivalence(condType, BooleanType()))
          error("Condition in while-statement must be boolean: " + cond)
        typecheck(body, NoType())

      case ReturnValueSt(expr) =>
        val t = typecheck(expr)
        if (!typeEquivalence(t, expected_type))
          error("Return type does not match expected function return type: " +t+ " vs" +expected_type)

      case ReturnSt() =>
        if (expected_type != NoType())
          error(s"Return statement in function expecting a return value ($expected_type) cannot have no return expression.")

      case _ => throw new Error("Wrong statement: "+e)
    } )
  }

  /** typecheck a definition */
  def typecheck ( e: Definition ) {
    trace(e,e match {
      case FuncDef(f,ps,ot,b)
      => st.insert(f,FuncDeclaration(ot,ps,"",0,0))
        st.begin_scope()
        ps.foreach{ case Bind(v,tp) => st.insert(v,VarDeclaration(tp,0,0)) }
        typecheck(b,ot)
        st.end_scope()

      /* PUT YOUR CODE HERE */

      case VarDef(v, t, expr) =>
        val et = typecheck(expr)
        val finalType = t match{
          case AnyType() => et
          case _ =>
            if (!typeEquivalence(t, et))
              error("Incompatible types in variable definition: " + v)
            t
        }
        st.insert(v, VarDeclaration(finalType, 0, 0))

      case TypeDef(name, t) =>
        st.insert(name,TypeDeclaration(t))

      case _ => throw new Error("Wrong statement: "+e)
    } )
  }

  /** typecheck the main program */
  def typecheck ( e: Program ) {
    typecheck(e.body,NoType())

  }
}