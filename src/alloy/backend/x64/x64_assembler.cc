/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <alloy/backend/x64/x64_assembler.h>

#include <alloy/backend/x64/tracing.h>
#include <alloy/backend/x64/x64_backend.h>
#include <alloy/backend/x64/x64_function.h>
#include <alloy/backend/x64/lir/lir_builder.h>
#include <alloy/backend/x64/lowering/lowering_table.h>
#include <alloy/backend/x64/optimizer/optimizer.h>
#include <alloy/backend/x64/optimizer/optimizer_passes.h>
#include <alloy/hir/hir_builder.h>
#include <alloy/hir/label.h>
#include <alloy/runtime/runtime.h>

using namespace alloy;
using namespace alloy::backend;
using namespace alloy::backend::x64;
using namespace alloy::backend::x64::lir;
using namespace alloy::backend::x64::optimizer;
using namespace alloy::hir;
using namespace alloy::runtime;


X64Assembler::X64Assembler(X64Backend* backend) :
    x64_backend_(backend),
    optimizer_(0),
    builder_(0),
    Assembler(backend) {
}

X64Assembler::~X64Assembler() {
  alloy::tracing::WriteEvent(EventType::AssemblerDeinit({
  }));

  delete optimizer_;
  delete builder_;
}

int X64Assembler::Initialize() {
  int result = Assembler::Initialize();
  if (result) {
    return result;
  }

  optimizer_ = new Optimizer(backend_->runtime());
  optimizer_->AddPass(new passes::RedundantMovPass());

  builder_ = new LIRBuilder(x64_backend_);

  alloy::tracing::WriteEvent(EventType::AssemblerInit({
  }));

  return result;
}

void X64Assembler::Reset() {
  builder_->Reset();
  optimizer_->Reset();
  string_buffer_.Reset();
  Assembler::Reset();
}

int X64Assembler::Assemble(
    FunctionInfo* symbol_info, HIRBuilder* hir_builder,
    DebugInfo* debug_info, Function** out_function) {
  int result = 0;

  // Lower HIR -> LIR.
  auto lowering_table = x64_backend_->lowering_table();
  result = lowering_table->Process(hir_builder, builder_);
  XEEXPECTZERO(result);

  // Stash raw LIR.
  if (debug_info) {
    builder_->Dump(&string_buffer_);
    debug_info->set_raw_lir_disasm(string_buffer_.ToString());
    string_buffer_.Reset();
  }

  // Optimize LIR.
  result = optimizer_->Optimize(builder_);
  XEEXPECTZERO(result);

  // Stash optimized LIR.
  if (debug_info) {
    builder_->Dump(&string_buffer_);
    debug_info->set_lir_disasm(string_buffer_.ToString());
    string_buffer_.Reset();
  }

  // Emit machine code.
  // TODO(benvanik): machine code.
  //result = emitter_->Emit(builder_, &machine_code, &length);
  XEEXPECTZERO(result);

  // Stash generated machine code.
  if (debug_info) {
    //emitter_->Dump(&string_buffer_);
    debug_info->set_machine_code_disasm(string_buffer_.ToString());
    string_buffer_.Reset();
  }

  X64Function* fn = new X64Function(symbol_info);
  fn->set_debug_info(debug_info);

  // TODO(benvanik): set mc

  *out_function = fn;

  result = 0;

XECLEANUP:
  Reset();
  return result;
}
