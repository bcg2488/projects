/****************************************************************************************************
 *
 * File: Code.scala
 * The IR code generator for SPL programs
 *
 ****************************************************************************************************/

package edu.uta.spl


abstract class CodeGenerator ( tc: TypeChecker )  {
  def typechecker: TypeChecker = tc
  def st: SymbolTable = tc.st
  def code ( e: Program ): IRstmt
  def allocate_variable ( name: String, var_type: Type, fname: String ): IRexp
}


class Code ( tc: TypeChecker ) extends CodeGenerator(tc) {

  var name_counter = 0

  /** generate a new name */
  def new_name ( name: String ): String = {
    name_counter += 1
    name + "_" + name_counter
  }

  /** IR code to be added at the end of program */
  var addedCode: List[IRstmt] = Nil

  def addCode ( code: IRstmt* ) {
    addedCode ++= code
  }

  /** allocate a new variable at the end of the current frame and return the access code */
  def allocate_variable ( name: String, var_type: Type, fname: String ): IRexp =
    st.lookup(fname) match {
      case Some(FuncDeclaration(rtp,params,label,level,min_offset))
        => // allocate variable at the next available offset in frame
           st.insert(name,VarDeclaration(var_type,level,min_offset))
           // the next available offset in frame is 4 bytes below
           st.replace(fname,FuncDeclaration(rtp,params,label,level,min_offset-4))
           // return the code that accesses the variable
           Mem(Binop("PLUS",Reg("fp"),IntValue(min_offset)))
      case _ => throw new Error("No current function: " + fname)
    }

  /** access a frame-allocated variable from the run-time stack */
  def access_variable ( name: String, level: Int ): IRexp =
    st.lookup(name) match {
      case Some(VarDeclaration(_,var_level,offset))
        => var res: IRexp = Reg("fp")
           // non-local variable: follow the static link (level-var_level) times
           for ( i <- var_level+1 to level )
               res = Mem(Binop("PLUS",res,IntValue(-8)))
           Mem(Binop("PLUS",res,IntValue(offset)))
      case _ => throw new Error("Undefined variable: " + name)
    }

  /** return the IR code from the Expr e (level is the current function nesting level,
   *  fname is the name of the current function/procedure) */
  def code ( e: Expr, level: Int, fname: String ): IRexp =
    e match {
      case BinOpExp(op,left,right)
        => val cl = code(left,level,fname)
           val cr = code(right,level,fname)
           val nop = op.toUpperCase()
           Binop(nop,cl,cr)
      case ArrayGen(len,v)
        => val A = allocate_variable(new_name("A"),typechecker.typecheck(e),fname)
           val L = allocate_variable(new_name("L"),IntType(),fname)
           val V = allocate_variable(new_name("V"),typechecker.typecheck(v),fname)
           val I = allocate_variable(new_name("I"),IntType(),fname)
           val loop = new_name("loop")
           val exit = new_name("exit")
           ESeq(Seq(List(Move(L,code(len,level,fname)),   // store length in L
                         Move(A,Allocate(Binop("PLUS",L,IntValue(1)))),
                         Move(V,code(v,level,fname)),     // store value in V
                         Move(Mem(A),L),                  // store length in A[0]
                         Move(I,IntValue(0)),
                         Label(loop),                     // for-loop
                         CJump(Binop("GEQ",I,L),exit),
                         Move(Mem(Binop("PLUS",A,Binop("TIMES",Binop("PLUS",I,IntValue(1)),IntValue(4)))),V),  // A[i] = v
                         Move(I,Binop("PLUS",I,IntValue(1))),
                         Jump(loop),
                         Label(exit))),
                A)

      /* PUT YOUR CODE HERE */

      case ArrayExp(elements) =>
        val A = allocate_variable(new_name("A"), typechecker.typecheck(e), fname)

        val initStmts = List(
          Move(A, Allocate(IntValue(elements.length+1))),
          Move(Mem(A), IntValue(elements.length))
        )
        val store = elements.zipWithIndex.map { case(elem, i) =>
          Move(Mem(Binop("PLUS", A, IntValue((i+1)*4))), code(elem, level, fname)
          )
        }
        val allStmts = initStmts ++ store
        ESeq(Seq(allStmts), A)

      case RecordExp(fields) =>
        val recType = typechecker.typecheck(e)
        val recordAddr = allocate_variable(new_name("record"), recType, fname)
        val recordType = typechecker.expandType(recType)

        val allocateInstr = recordType match {
          case RecordType(fieldTypes) =>
            Move(recordAddr, Allocate(IntValue(fieldTypes.length)))
          case _ => throw new Error(s"Not a record type: $recType")
        }

        val fieldMoves = recordType match {
          case RecordType(fieldTypes) =>
            fields.map {
              case Bind(name, value) =>
                val index = fieldTypes.indexWhere(_.name == name)
                if (index < 0) throw new Error(s"Field $name not found in record type")
                val fieldAddr = Binop("PLUS", recordAddr, IntValue(index * 4))
                Move(Mem(fieldAddr), code(value, level, fname))
            }
          case _ => throw new Error(s"Not a record type: $recType")
        }

        ESeq(Seq(allocateInstr +: fieldMoves), recordAddr)



      case LvalExp(lv) => code(lv, level, fname)

      case IntConst(i) =>
        IntValue(i)

      case FloatConst(f) =>
        FloatValue(f)

      case BooleanConst(b) =>
        IntValue(if (b) 1 else 0)

      case StringConst(s) =>
        StringValue(s)

      case NullExp() =>
        IntValue(0)

      case CallExp(f, args) =>
        st.lookup(f) match {
          case Some(FuncDeclaration(_, _, lbl, funcLevel, _)) =>
            val static_link: IRexp = if (funcLevel == level + 1) Reg("fp")
            else if (funcLevel == level) Mem(Binop("PLUS", Reg("fp"), IntValue(-8)))
            else {
              var statlink: IRexp = Reg("fp")
              for (_ <- 1 to (level - funcLevel )) {
                statlink = Mem(Binop("PLUS", statlink, IntValue(-8)))
              }
              statlink
            }
            Call(lbl, static_link, args.map(arg => code(arg, level, fname)))
          case _ => throw new Error(s"Function $f not found")

        }

      case UnOpExp(op, e) =>
        val v = code(e, level, fname)
        op match {
          case "minus" => Unop("MINUS", v)
          case _       => throw new Error(s"Unsupported unary op: $op")
        }




      case _ => throw new Error("Wrong expression: "+e)
    }

  /** return the IR code from the Lvalue e (level is the current function nesting level,
   *  fname is the name of the current function/procedure) */
  def code ( e: Lvalue, level: Int, fname: String ): IRexp =
    e match {
     case RecordDeref(r,a)
        => val cr = code(r,level,fname)
           typechecker.expandType(typechecker.typecheck(r)) match {
              case RecordType(cl)
                => val i = cl.map(_.name).indexOf(a)
                   Mem(Binop("PLUS",cr,IntValue(i*4)))
              case _ => throw new Error("Unkown record: "+e)
           }

     /* PUT YOUR CODE HERE */

     case Var(x) =>
       access_variable(x, level)

     case ArrayDeref(arrayExpr, indexExpr) =>
       val base = code(arrayExpr, level, fname)
       val index = code(indexExpr, level, fname)
       typechecker.typecheck(arrayExpr) match {
         case ArrayType(_) =>
           Mem(Binop("PLUS", base, Binop("TIMES", Binop("PLUS", index, IntValue(1)), IntValue(4))))
         case _ => throw new Error("Array indexing can only be done on Arrays")
       }


     case _ => throw new Error("Wrong statement: " + e)
    }

  /** return the IR code from the Statement e (level is the current function nesting level,
   *  fname is the name of the current function/procedure)
   *  and exit_label is the exit label       */
  def code ( e: Stmt, level: Int, fname: String, exit_label: String ): IRstmt =
    e match {
      case ForSt(v,a,b,c,s)
        => val loop = new_name("loop")
           val exit = new_name("exit")
           val cv = allocate_variable(v,IntType(),fname)
           val ca = code(a,level,fname)
           val cb = code(b,level,fname)
           val cc = code(c,level,fname)
           val cs = code(s,level,fname,exit)
           Seq(List(Move(cv,ca),  // needs cv, not Mem(cv)
                    Label(loop),
                    CJump(Binop("GT",cv,cb),exit),
                    cs,
                    Move(cv,Binop("PLUS",cv,cc)),  // needs cv, not Mem(cv)
                    Jump(loop),
                    Label(exit)))

      /* PUT YOUR CODE HERE */

      case BlockSt(list_defs, list_stmts) =>
        st.begin_scope()
        val defIR = list_defs.flatMap {
          case null => None
          case d => Some(code(d, fname, level))
        }
        val stmtIR = list_stmts.flatMap {
          case null => None
          case stmt => Some(code(stmt, level, fname, exit_label))
        }
        st.end_scope()
        Seq(defIR ::: stmtIR)

      case null => throw new Error("Null statement encountered in block")


      case PrintSt(args) =>
        val arg_syscall = args.map { item =>
          val item_type = tc.typecheck(item)
          val IR = code(item, level, fname)
          val sys_op = item_type match {
            case StringType() => "WRITE_STRING"
            case IntType() => "WRITE_INT"
            case FloatType() => "WRITE_FLOAT"
            case BooleanType() => "WRITE_BOOL"
            case _ => "WRITE_INT"
          }
          SystemCall(sys_op, IR)
        }
        Seq( arg_syscall :+ SystemCall("WRITE_STRING", StringValue("\\n")))


      case CallSt(f, args) =>
        st.lookup(f) match {
          case Some(FuncDeclaration(_, _, label, funcLevel, _)) =>
            val evaluatedArgs = args.map(arg => code(arg, level, fname))
            val static_link: IRexp = if (funcLevel == level + 1) Reg("fp")
            else if (funcLevel == level) {
              var sr: IRexp = Reg("fp")
              Mem(Binop("PLUS", sr, IntValue(-8)))

            } else {
              var statlink: IRexp = Reg("fp")
              for (_ <- 1 to (level - funcLevel )+1) {
                statlink = Mem(Binop("PLUS", statlink, IntValue(-8)))
              }
              statlink
            }
            CallP(label, static_link, evaluatedArgs)

          case Some(_: VarDeclaration) =>
            throw new Error(s"$f is not a function and cannot be called")

          case Some(_: TypeDeclaration) =>
            throw new Error(s"$f is a type name, not a function")

          case None => throw new Error("Not here")
        }


      case ReadSt(args) =>
        val arg_syscall = args.map { item =>
          val item_type = tc.typecheck(item)
          val syscall_op = item_type match {
            case IntType() => "READ_INT"
            case FloatType() => "READ_FLOAT"
            case BooleanType() => "READ_BOOL"
            case StringType() => "READ_STRING"
            case _ => "READ_INT"  // Default to "READ_INT" if the type is unrecognized
          }
          val access = item match {
            case Var(x) => access_variable(x, level)
            case ArrayDeref(base, index) =>
              val base_addr = code(base, level, fname)
              val idx = code(index, level, fname)
              Mem(Binop("PLUS", base_addr, Binop("TIMES", Binop("PLUS", idx, IntValue(1)), IntValue(4))))
            case RecordDeref(base, field) =>
              val base_addr = code(base, level, fname)
              typechecker.expandType(typechecker.typecheck(base)) match {
                case RecordType(fields) =>
                  val offset = fields.map(_.name).indexOf(field)
                  Mem(Binop("PLUS", base_addr, IntValue(offset * 4)))
                case _ => throw new Error("Invalid record deref in ReadSt")
              }
          }
          SystemCall(syscall_op, access)
        }
        Seq(arg_syscall)



      case IfSt(cond, then_st, else_st) =>
        val tlabel = new_name("cont")
        val elabel = new_name("exit")
        val cc = code(cond, level, fname)
        val cs1 = code(then_st, level, fname, exit_label)
        val cs2 = if (else_st == null) Seq(List()) else code(else_st, level, fname, exit_label)
        Seq(List(
          CJump(cc, elabel),
          cs2,
          Jump(tlabel),
          Label(elabel),
          cs1,
          Label(tlabel)
        ))

      case WhileSt(cond, body) =>
        val test = new_name("loop")
        val end = new_name("exit")
        val cc = code(cond, level, fname)
        val cb = code(body, level, fname, end)
        Seq(List(
          Label(test),
          CJump(Unop("NOT", cc), end),
          cb,
          Jump(test),
          Label(end)
        ))

      case AssignSt(lval, expr) =>
        val target = code(lval, level, fname)
        val value = code(expr, level, fname)
        Move(target, value)


      case ReturnValueSt(e) =>
        Seq(List(Move(Reg("a0"), code(e, level, fname)),
                Move(Reg("ra"), Mem(Binop("PLUS", Reg("fp"), IntValue(-4)))),
                Move(Reg("sp"), Reg("fp")),
                Move(Reg("fp"), Mem(Reg("fp"))),
                Return()))

      case ReturnSt() =>
        Seq(List(Move(Reg("ra"), Mem(Binop("PLUS", Reg("fp"), IntValue(-4)))),
                Move(Reg("sp"), Reg("fp")),
                Move(Reg("fp"), Mem(Reg("fp"))),
                Return()))

      case _ => throw new Error("Wrong statement: " + e)
   }

  /** return the IR code for the declaration block of function fname
   * (level is the current function nesting level) */
  def code ( e: Definition, fname: String, level: Int ): IRstmt =
    e match {
      case FuncDef(f,ps,ot,b)
        => val flabel = if (f == "main") f else new_name(f)
           /* initial available offset in frame f is -12 */
           st.insert(f,FuncDeclaration(ot,ps,flabel,level+1,-12))
           st.begin_scope()
           /* formal parameters have positive offsets */
           ps.zipWithIndex.foreach{ case (Bind(v,tp),i)
                                      => st.insert(v,VarDeclaration(tp,level+1,(ps.length-i)*4)) }
           val body = code(b,level+1,f,"")
           st.end_scope()
           st.lookup(f) match {
             case Some(FuncDeclaration(_,_,_,_,offset))
               => addCode(Label(flabel),
                          /* prologue */
                          Move(Mem(Reg("sp")),Reg("fp")),
                          Move(Reg("fp"),Reg("sp")),
                          Move(Mem(Binop("PLUS",Reg("fp"),IntValue(-4))),Reg("ra")),
                          Move(Mem(Binop("PLUS",Reg("fp"),IntValue(-8))),Reg("v0")),
                          Move(Reg("sp"),Binop("PLUS",Reg("sp"),IntValue(offset))),
                          body,
                          /* epilogue */
                          Move(Reg("ra"),Mem(Binop("PLUS",Reg("fp"),IntValue(-4)))),
                          Move(Reg("sp"),Reg("fp")),
                          Move(Reg("fp"),Mem(Reg("fp"))),
                          Return())
                  Seq(List())
             case _ => throw new Error("Unknown function: "+f)
           }

      /* PUT YOUR CODE HERE */

      case TypeDef(name, typ) =>
        st.insert(name, TypeDeclaration(typ))
        Seq(List())

      case VarDef(name, hasType, value) =>
        val typ = if (hasType.isInstanceOf[AnyType])
          typechecker.typecheck(value)
        else
          hasType
        val v = allocate_variable(name, typ, fname)
        Move(v, code(value, level, fname))


      case _ => throw new Error("Wrong statement: " + e)
    }

    def code ( e: Program ): IRstmt =
      e match {
        case Program(b@BlockSt(_,_))
          => st.begin_scope()
             val res = code(FuncDef("main",List(),NoType(),b),"",0)
             st.end_scope()
             Seq(res::addedCode)
        case _ => throw new Error("Wrong program "+e);
      }
}
