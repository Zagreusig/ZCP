#include "debug/ASTPrinter.h"
#include "debug/IRDebug.h"
#include "driver/compiler.h"
#include "utils/file_util.h"
#include <iostream>
#include <string>

int failed = 0;

void CHECK(bool cond, const std::string& msg) {
   if (!cond) { std::cerr << "Failed: " << msg << std::endl; failed++; }
}


void test_toks() {
   for (size_t i = 0; i < static_cast<size_t>(TokenType::COUNT); i++) {
      TokenType t = static_cast<TokenType>(i);
      Token tok = [&] {
         switch (t) {
            case TokenType::INT_LIT:    return tok::make_int(1, 0, 0, 0);
            case TokenType::CHAR_LIT:   return tok::make_char('a', 0, 0, 0);
            case TokenType::STR_LIT:    return tok::make_str("test str", 0, 0, 0);
            case TokenType::IDENTIFIER: return tok::make_ident("test ident", 0, 0, 0);
            case TokenType::BOOL:       return tok::make_bool(true, 0, 0, 0);
            default:                    return tok::make(t, 0, 0, 0);
         }
      }();
      try {
         tok.spelling(); tok.name(); tok.value_str();
      } catch (const std::exception& e) {
         CHECK(false, "TokenType " + tok.name() + " threw: " + e.what());
      }
   }

   Token bad = tok::make_int(-1, 0, 0, 0);
   try { bad.text(); CHECK(false, "int_val token .text() should have thrown."); }
   catch (const BadVariantAccess&) { /* expected result */ }
   catch (...) { CHECK(false, ".text() threw the wrong exception type"); }
}

void test_nodes() {
   Compiler compiler("", {}, "out", "selftest");
   auto& A = compiler.allocator;

   auto wrap_expr = [&](auto* n) { NodeExpr* e = A.alloc<NodeExpr>(); e->variant = n; return e; };
   auto wrap_stmt = [&](auto* n) { NodeStmt* s = A.alloc<NodeStmt>(); s->variant = n; return s; };
   auto wrap_top  = [&](auto* n) { NodeTopLevel* t = A.alloc<NodeTopLevel>(); t->variant = n; return t; };
   auto wrap_cond = [&](auto* n) { NodeCondition* c = A.alloc<NodeCondition>(); c->variant = n; return c; };

   auto print_one_stmt = [&](NodeStmt* s) {
      NodeScopeBlock* body = A.alloc<NodeScopeBlock>();
      body->stmts.push_back(s);
      NodeFunction* fn = A.alloc<NodeFunction>();
      fn->name = tok::make_ident("f", 0, 0, 0);
      fn->body = body;
      NodeProg prog; prog.declarations.push_back(wrap_top(fn));
      std::ostringstream out;
      ASTPrinter(prog, out).print();
      return out.str();
   };

   auto print_one_expr = [&](NodeExpr* e) {
      NodeStmtExpr* se = A.alloc<NodeStmtExpr>(); se->expr = e;
      return print_one_stmt(wrap_stmt(se));
   };

   // For top-level decls (struct decls) that aren't nested inside a function body.
   auto print_one_top = [&](NodeTopLevel* t) {
      NodeProg prog; prog.declarations.push_back(t);
      std::ostringstream out;
      ASTPrinter(prog, out).print();
      return out.str();
   };


   auto check_expr = [&](const std::string& label, auto build) {
      try {
         CHECK(!print_one_expr(wrap_expr(build())).empty(), label + " failed.");
      } catch (const std::exception& e) {
         CHECK(false, label + " threw: " + std::string(e.what()));
      }
   };
   auto check_stmt = [&](const std::string& label, auto build) {
      try {
         CHECK(!print_one_stmt(wrap_stmt(build())).empty(), label + " failed.");
      } catch (const std::exception& e) {
         CHECK(false, label + " threw: " + std::string(e.what()));
      }
   };

   auto build_int  = [&]{ auto* i = A.alloc<NodeExprIntLit>(); i->INT_LIT = tok::make_int(1,0,0,0); return i; };
   auto build_ident = [&](const std::string& name) { auto* i = A.alloc<NodeExprIdent>(); i->ident = tok::make_ident(name,0,0,0); return i; };


   check_expr("NodeExprIntLit",  [&]{ auto* n = A.alloc<NodeExprIntLit>();  n->INT_LIT = tok::make_int(1,0,0,0);     return n; });
   check_expr("NodeExprCharLit", [&]{ auto* n = A.alloc<NodeExprCharLit>(); n->CHAR_LIT = tok::make_char('a',0,0,0); return n; });
   check_expr("NodeExprStrLit",  [&]{ auto* n = A.alloc<NodeExprStrLit>();  n->STR_LIT = tok::make_str("str",0,0,0); return n; });
   check_expr("NodeExprBoolLit", [&]{ auto* n = A.alloc<NodeExprBoolLit>(); n->BOOL_LIT = tok::make(TokenType::TRUE,0,0,0); return n; });
   check_expr("NodeExprArrayLit", [&]{ auto* n = A.alloc<NodeExprArrayLit>(); return n; });
   check_expr("NodeExprIndex", [&]{ auto* n = A.alloc<NodeExprIndex>(); n->ident = tok::make_ident("ident",0,0,0);
      n->index = wrap_expr(build_int()); return n; });
   check_expr("NodeExprRead",   [&]{ auto* n = A.alloc<NodeExprRead>();   n->kind = DataType::INT; return n; });
   check_expr("NodeExprIncDec", [&]{ auto* n = A.alloc<NodeExprIncDec>(); n->ident = tok::make_ident("x",0,0,0);
      n->is_increment = true; n->is_prefix = false; return n; });
   check_expr("NodeBinExpr", [&]{ auto* n = A.alloc<NodeBinExpr>(); n->operation = BinExprType::ADDITION;
      n->left = wrap_expr(build_int()); n->right = wrap_expr(build_int()); return n; });
   check_expr("NodeExprCall", [&]{ auto* n = A.alloc<NodeExprCall>(); n->name = tok::make_ident("f",0,0,0);
      n->args = { wrap_expr(build_int()) }; return n; });
   check_expr("NodeExprUnary", [&]{ auto* n = A.alloc<NodeExprUnary>(); n->op = UnaryExprType::NEGATE;
      n->operand = wrap_expr(build_int()); return n; });
   check_expr("NodeExprNew", [&]{ auto* n = A.alloc<NodeExprNew>(); n->type_name = tok::make_ident("Foo",0,0,0); return n; });

   // NodeExprField - also the regression check for ASTPrinter.cpp's .int_val()->.text() fix.
   try {
      auto* n = A.alloc<NodeExprField>();
      n->base       = wrap_expr(build_ident("obj"));
      n->field      = tok::make_ident("member",0,0,0);
      std::string out = print_one_expr(wrap_expr(n));
      CHECK(!out.empty(), "NodeExprField failed.");
      CHECK(out.find("member") != std::string::npos, "NodeExprField output missing field name - printer regression?");
   } catch (const std::exception& e) { CHECK(false, "NodeExprField threw: " + std::string(e.what())); }

   // ---- NodeStmt (11) ----
   check_stmt("NodeStmtExit", [&]{ auto* n = A.alloc<NodeStmtExit>(); n->expr = wrap_expr(build_int()); return n; });
   check_stmt("NodeStmtExpr", [&]{ auto* n = A.alloc<NodeStmtExpr>(); n->expr = wrap_expr(build_int()); return n; });
   check_stmt("NodeStmtHave", [&]{
      auto* n = A.alloc<NodeStmtHave>();
      n->decl.name = tok::make_ident("x",0,0,0);
      n->decl.type = TypeInfo{ .base = DataType::INT };
      n->expr = wrap_expr(build_int());
      return n;
   });
   check_stmt("NodeScopeBlock", [&]{
      auto* n = A.alloc<NodeScopeBlock>();
      NodeStmtExpr* inner = A.alloc<NodeStmtExpr>(); inner->expr = wrap_expr(build_int());
      n->stmts.push_back(wrap_stmt(inner));
      return n;
   });
   check_stmt("NodeStmtIf", [&]{
      auto* n = A.alloc<NodeStmtIf>();
      auto* cmp = A.alloc<NodeCmpCondition>();
      cmp->operation = ComparisonOp::EQUAL; cmp->left = wrap_expr(build_int()); cmp->right = wrap_expr(build_int());
      n->condition = wrap_cond(cmp);
      n->body = A.alloc<NodeScopeBlock>();
      return n;
   });
   check_stmt("NodeStmtWhile", [&]{
      auto* n = A.alloc<NodeStmtWhile>();
      auto* cmp = A.alloc<NodeCmpCondition>();
      cmp->operation = ComparisonOp::LESS_THAN; cmp->left = wrap_expr(build_int()); cmp->right = wrap_expr(build_int());
      n->condition = wrap_cond(cmp);
      n->body = A.alloc<NodeScopeBlock>();
      return n;
   });
   check_stmt("NodeStmtAssign", [&]{
      auto* n = A.alloc<NodeStmtAssign>();
      n->ident  = tok::make_ident("x",0,0,0);
      n->target = wrap_expr(build_ident("x"));
      n->expr   = wrap_expr(build_int());
      return n;
   });
   check_stmt("NodeStmtFor", [&]{
      auto* n = A.alloc<NodeStmtFor>();

      auto* init = A.alloc<NodeStmtHave>();
      init->decl.name = tok::make_ident("i",0,0,0);
      init->decl.type = TypeInfo{ .base = DataType::INT };
      init->expr = wrap_expr(build_int());
      n->init = wrap_stmt(init);

      auto* inc = A.alloc<NodeStmtAssign>();
      inc->ident  = tok::make_ident("i",0,0,0);
      inc->target = wrap_expr(build_ident("i"));
      inc->expr   = wrap_expr(build_int());
      n->increment = wrap_stmt(inc);

      auto* cmp = A.alloc<NodeCmpCondition>();
      cmp->operation = ComparisonOp::LESS_THAN; cmp->left = wrap_expr(build_ident("i")); cmp->right = wrap_expr(build_int());
      n->condition = wrap_cond(cmp);
      n->body = A.alloc<NodeScopeBlock>();
      return n;
   });
   check_stmt("NodeStmtReturn", [&]{ auto* n = A.alloc<NodeStmtReturn>(); n->expr = wrap_expr(build_int()); return n; });
   check_stmt("NodeStmtScope",  [&]{ auto* n = A.alloc<NodeStmtScope>(); n->scope = A.alloc<NodeScopeBlock>(); return n; });
   check_stmt("NodeStmtPrint",  [&]{ auto* n = A.alloc<NodeStmtPrint>(); n->expr = wrap_expr(build_int()); n->nwln = true; return n; });

   // ---- NodeCondition (CmpCondition already exercised via If/While/For above) ----
   check_stmt("NodeLogicCondition", [&]{
      auto* left = A.alloc<NodeCmpCondition>();
      left->operation = ComparisonOp::EQUAL; left->left = wrap_expr(build_int()); left->right = wrap_expr(build_int());
      auto* right = A.alloc<NodeCmpCondition>();
      right->operation = ComparisonOp::EQUAL; right->left = wrap_expr(build_int()); right->right = wrap_expr(build_int());
      auto* logic = A.alloc<NodeLogicCondition>();
      logic->operation = LogicOp::AND;
      logic->left  = wrap_cond(left);
      logic->right = wrap_cond(right);

      auto* n = A.alloc<NodeStmtIf>();
      n->condition = wrap_cond(logic);
      n->body = A.alloc<NodeScopeBlock>();
      return n;
   });

   // ---- NodeTypeDecl / NodeTopLevel::TypeDecl (StructDecl) ----
   try {
      auto* sd = A.alloc<NodeStructDecl>();
      sd->name = tok::make_ident("Point",0,0,0);
      NodeStructField f;
      f.decl.name = tok::make_ident("x",0,0,0);
      f.decl.type = TypeInfo{ .base = DataType::INT };
      sd->vars.push_back(f);
      auto* td = A.alloc<NodeTypeDecl>(); td->variant = sd;
      CHECK(!print_one_top(wrap_top(td)).empty(), "NodeStructDecl failed.");
   } catch (const std::exception& e) { CHECK(false, "NodeStructDecl threw: " + std::string(e.what())); }

   // Deliberate edge case: a struct field with no type set. ASTPrinter.cpp:78's
   // `var.decl.type.value()` trusts the parser's invariant unconditionally - confirm violating
   // it throws a catchable std::bad_optional_access instead of crashing (documents current
   // behavior; the invariant itself is the parser's responsibility, not fixed here).
   try {
      auto* sd = A.alloc<NodeStructDecl>();
      sd->name = tok::make_ident("Bad",0,0,0);
      NodeStructField f;
      f.decl.name = tok::make_ident("x",0,0,0);
      f.decl.type = std::nullopt;
      sd->vars.push_back(f);
      auto* td = A.alloc<NodeTypeDecl>(); td->variant = sd;
      print_one_top(wrap_top(td));
      CHECK(false, "NodeStructDecl with unset field type should have thrown.");
   } catch (const std::bad_optional_access&) { /* expected */ }
   catch (const std::exception& e) { CHECK(false, "NodeStructDecl(nullopt) threw the wrong exception: " + std::string(e.what())); }

   // NodeTopLevel::Function is exercised implicitly by every check_expr/check_stmt call above
   // (print_one_stmt always wraps its argument in a NodeFunction to reach the public print()
   // entry point) - no separate case needed.
}

void test_ir() {
   IRFunction fn; fn.name = "f"; fn.ret_type = IRType::I64;
   IRBasicBlock& blk = fn.new_block("entry"); // One block only - safe to hold ref.
   
   VReg d = fn.fresh_vreg(IRType::I64);
   // Const
   { IRInstruction i(IROp::Const, d); i.operands = {IROperand::make_const(5)}; blk.instructions.push_back(i); }
   // Copy
   { IRInstruction i(IROp::Copy, fn.fresh_vreg(IRType::I64)); i.operands = {IROperand::make_reg(d)}; blk.instructions.push_back(i); }
   // Arith / Compare / Log grouping
   { VReg a = fn.fresh_vreg(IRType::I64); VReg b = fn.fresh_vreg(IRType::I64); IRInstruction i(IROp::Add); i.operands = {IROperand::make_reg(a), IROperand::make_reg(b)}; blk.instructions.push_back(i); }
   // Not
   { IRInstruction i(IROp::Not, d); i.operands = {IROperand::make_reg(d)}; blk.instructions.push_back(i); }
   // Alloca
   { VReg ptr = fn.fresh_vreg(IRType::Ptr); IRInstruction i(IROp::Alloca, ptr); i.imm = 8; blk.instructions.push_back(i); }
   // Load
   { IRInstruction i(IROp::Load, d); i.operands = {IROperand::make_reg(d)}; blk.instructions.push_back(i); }
   // Store
   { IRInstruction i(IROp::Store); i.operands = {IROperand::make_reg(d), IROperand::make_reg(d)}; blk.instructions.push_back(i); }
   // GlobalAddr
   { IRInstruction i(IROp::GlobalAddr); i.operands = {IROperand::make_symbol("g")}; blk.instructions.push_back(i); }
   // GetElemPtr
   { IRInstruction i(IROp::GetElemPtr); i.operands = {IROperand::make_reg(d), IROperand::make_reg(fn.fresh_vreg(IRType::I64))}; i.imm = 8; blk.instructions.push_back(i); }
   // Call
   { IRInstruction i(IROp::Call); i.operands = { IROperand::make_symbol("f"), IROperand::make_reg(fn.fresh_vreg(IRType::I64))}; blk.instructions.push_back(i); }
   // CallResult
   { IRInstruction i(IROp::CallResult); blk.instructions.push_back(i); }
   // Br - branch
   { IRInstruction i(IROp::Br); i.operands = {IROperand::make_block(0)}; blk.instructions.push_back(i); }
   // CondBr - operands[0] = condition, [1] = then, [2] = else
   { IRInstruction i(IROp::CondBr); i.operands = {IROperand::make_reg(d), IROperand::make_block(1), IROperand::make_block(2)}; blk.instructions.push_back(i); }
   // Ret
   { IRInstruction i(IROp::Ret); blk.instructions.push_back(i); i.operands = { IROperand::make_reg(d) }; blk.instructions.push_back(i); }
   // Exit
   { IRInstruction i(IROp::Exit); blk.instructions.push_back(i); }

   IRModule mod; mod.functions.push_back(fn); 
   std::ostringstream out; IRPrinter(out).print(mod);
   CHECK(!out.str().empty(), "IRLowering issue, out.str().empty() was true.");
}

void test_err_pths() {
   // auto src = read_file("tests/struct_test.z");
   // Compiler c1(src.value(), {}, "out", "struct_test.z");
   // c1.run();
   // CHECK(!c1.diagnostics.has_errors(), "struct_test.z should compile clean"); // currently untrue.

   Compiler c2("fn main() { print(x); }", {}, "out", "undeclared.z");
   c2.run();
   CHECK(c2.diagnostics.has_errors(), "undeclared identifier should error");

   Compiler c3("fn f(int a) {} fn main() { f(1, 2, 3); }", {}, "out", "arity.z");
   c3.run();
   CHECK(c3.diagnostics.has_errors(), "wrong-arity call should error");
}

int main() {
   test_toks(); test_nodes(); test_ir(); test_err_pths();
   std::cout << (failed == 0 ? "All check passed.\n" : std::to_string(failed) + " checks(s) failed.\n");
   return failed > 0 ? 1 : 0;
}