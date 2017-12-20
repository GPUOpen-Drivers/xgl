//===- SPIRVInstruction.cpp -Class to represent SPIR-V instruction - C++ -*-===//
//
//                     The LLVM/SPIRV Translator
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Copyright (c) 2014 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimers.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimers in the documentation
// and/or other materials provided with the distribution.
// Neither the names of Advanced Micro Devices, Inc., nor the names of its
// contributors may be used to endorse or promote products derived from this
// Software without specific prior written permission.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
// THE SOFTWARE.
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements SPIR-V instructions.
///
//===----------------------------------------------------------------------===//

#include "hex_float.h"
#include "SPIRVInstruction.h"
#include "SPIRVBasicBlock.h"
#include "SPIRVFunction.h"

#include <unordered_set>

namespace SPIRV {

// Complete constructor for instruction with type and id
SPIRVInstruction::SPIRVInstruction(unsigned TheWordCount, Op TheOC,
    SPIRVType *TheType, SPIRVId TheId, SPIRVBasicBlock *TheBB)
  :SPIRVValue(TheBB->getModule(), TheWordCount, TheOC, TheType, TheId),
   BB(TheBB){
  validate();
}

SPIRVInstruction::SPIRVInstruction(unsigned TheWordCount, Op TheOC,
  SPIRVType *TheType, SPIRVId TheId, SPIRVBasicBlock *TheBB, SPIRVModule *TheBM)
  : SPIRVValue(TheBM, TheWordCount, TheOC, TheType, TheId), BB(TheBB){
  validate();
}

// Complete constructor for instruction with id but no type
SPIRVInstruction::SPIRVInstruction(unsigned TheWordCount, Op TheOC,
    SPIRVId TheId, SPIRVBasicBlock *TheBB)
  :SPIRVValue(TheBB->getModule(), TheWordCount, TheOC, TheId), BB(TheBB){
  validate();
}
// Complete constructor for instruction without type and id
SPIRVInstruction::SPIRVInstruction(unsigned TheWordCount, Op TheOC,
    SPIRVBasicBlock *TheBB)
  :SPIRVValue(TheBB->getModule(), TheWordCount, TheOC), BB(TheBB){
  validate();
}
// Complete constructor for instruction with type but no id
SPIRVInstruction::SPIRVInstruction(unsigned TheWordCount, Op TheOC,
    SPIRVType *TheType, SPIRVBasicBlock *TheBB)
  :SPIRVValue(TheBB->getModule(), TheWordCount, TheOC, TheType), BB(TheBB){
  validate();
}

void
SPIRVInstruction::setParent(SPIRVBasicBlock *TheBB) {
  assert(TheBB && "Invalid BB");
  if (BB == TheBB)
    return;
  assert(BB == NULL && "BB cannot change parent");
  BB = TheBB;
}

void
SPIRVInstruction::setScope(SPIRVEntry *Scope) {
  assert(Scope && Scope->getOpCode() == OpLabel && "Invalid scope");
  setParent(static_cast<SPIRVBasicBlock*>(Scope));
}

SPIRVFunctionCall::SPIRVFunctionCall(SPIRVId TheId, SPIRVFunction *TheFunction,
    const std::vector<SPIRVWord> &TheArgs, SPIRVBasicBlock *BB)
  :SPIRVFunctionCallGeneric(
      TheFunction->getFunctionType()->getReturnType(),
      TheId, TheArgs, BB), FunctionId(TheFunction->getId()){
  validate();
}

void
SPIRVFunctionCall::validate()const {
  SPIRVFunctionCallGeneric::validate();
}

// ToDo: Each instruction should implement this function
std::vector<SPIRVValue *>
SPIRVInstruction::getOperands() {
  std::vector<SPIRVValue *> Empty;
  assert(0 && "not supported");
  return Empty;
}

std::vector<SPIRVType*>
SPIRVInstruction::getOperandTypes(const std::vector<SPIRVValue *> &Ops) {
  std::vector<SPIRVType*> Tys;
  for (auto& I : Ops) {
    SPIRVType* Ty = nullptr;
    if (I->getOpCode() == OpFunction)
      Ty = reinterpret_cast<SPIRVFunction*>(I)->getFunctionType();
    else
      Ty = I->getType();

    Tys.push_back(Ty);
  }
  return Tys;
}

std::vector<SPIRVType*>
SPIRVInstruction::getOperandTypes() {
  return getOperandTypes(getOperands());
}

bool
isSpecConstantOpAllowedOp(Op OC) {
  static SPIRVWord Table[] =
  {
    OpSConvert,
    OpFConvert,
    OpConvertFToS,
    OpConvertSToF,
    OpConvertFToU,
    OpConvertUToF,
    OpUConvert,
    OpConvertPtrToU,
    OpConvertUToPtr,
    OpGenericCastToPtr,
    OpPtrCastToGeneric,
    OpBitcast,
    OpQuantizeToF16,
    OpSNegate,
    OpNot,
    OpIAdd,
    OpISub,
    OpIMul,
    OpUDiv,
    OpSDiv,
    OpUMod,
    OpSRem,
    OpSMod,
    OpShiftRightLogical,
    OpShiftRightArithmetic,
    OpShiftLeftLogical,
    OpBitwiseOr,
    OpBitwiseXor,
    OpBitwiseAnd,
    OpFNegate,
    OpFAdd,
    OpFSub,
    OpFMul,
    OpFDiv,
    OpFRem,
    OpFMod,
    OpVectorShuffle,
    OpCompositeExtract,
    OpCompositeInsert,
    OpLogicalOr,
    OpLogicalAnd,
    OpLogicalNot,
    OpLogicalEqual,
    OpLogicalNotEqual,
    OpSelect,
    OpIEqual,
    OpINotEqual,
    OpULessThan,
    OpSLessThan,
    OpUGreaterThan,
    OpSGreaterThan,
    OpULessThanEqual,
    OpSLessThanEqual,
    OpUGreaterThanEqual,
    OpSGreaterThanEqual,
    OpAccessChain,
    OpInBoundsAccessChain,
    OpPtrAccessChain,
    OpInBoundsPtrAccessChain,
  };
  static std::unordered_set<SPIRVWord>
    Allow(std::begin(Table), std::end(Table));
  return Allow.count(OC);
}

SPIRVSpecConstantOp *
createSpecConstantOpInst(SPIRVInstruction *Inst) {
  auto OC = Inst->getOpCode();
  assert(isSpecConstantOpAllowedOp(OC) &&
      "Op code not allowed for OpSpecConstantOp");
  auto Ops = Inst->getIds(Inst->getOperands());
  Ops.insert(Ops.begin(), OC);
  return static_cast<SPIRVSpecConstantOp *>(
    SPIRVSpecConstantOp::create(OpSpecConstantOp, Inst->getType(),
        Inst->getId(), Ops, nullptr, Inst->getModule()));
}

SPIRVInstruction *
createInstFromSpecConstantOp(SPIRVSpecConstantOp *Inst) {
  assert(Inst->getOpCode() == OpSpecConstantOp &&
      "Not OpSpecConstantOp");
  auto Ops = Inst->getOpWords();
  auto OC = static_cast<Op>(Ops[0]);
  assert (isSpecConstantOpAllowedOp(OC) &&
      "Op code not allowed for OpSpecConstantOp");
  Ops.erase(Ops.begin(), Ops.begin() + 1);
  return SPIRVInstTemplateBase::create(OC, Inst->getType(),
      Inst->getId(), Ops, nullptr, Inst->getModule());
}

uint64_t
getConstantValue(SPIRVValue *BV, uint32_t I = 0) {
  assert(BV->getType()->isTypeScalar() || BV->getType()->isTypeVector());
  uint64_t ConstVal = 0;
  if (BV->getOpCode() == OpConstant ||
      BV->getOpCode() == OpSpecConstant)
    ConstVal = static_cast<SPIRVConstant *>(BV)->getZExtIntValue();
  else if (BV->getOpCode() == OpConstantTrue ||
           BV->getOpCode() == OpSpecConstantTrue)
    ConstVal = static_cast<SPIRVConstantTrue *>(BV)->getBoolValue();
  else if (BV->getOpCode() == OpConstantFalse ||
           BV->getOpCode() == OpSpecConstantFalse)
    ConstVal = static_cast<SPIRVConstantTrue *>(BV)->getBoolValue();
  else if (BV->getOpCode() == OpConstantComposite ||
           BV->getOpCode() == OpSpecConstantComposite)
    ConstVal = getConstantValue(
      static_cast<SPIRVConstantComposite *>(BV)->getElements()[I]);
  else if (BV->getOpCode() == OpConstantNull ||
           BV->getOpCode() == OpUndef)
    ConstVal = 0;
  else
    llvm_unreachable("Invalid op code");
  return ConstVal;
}

SPIRVValue *
constantCompositeExtract(SPIRVValue *Composite, SPIRVType *ObjectTy,
  std::vector<uint32_t> &Indices) {
  SPIRVModule *BM = Composite->getModule();
  SPIRVType *CompositeTy = Composite->getType();
  assert(CompositeTy->isTypeComposite());

  for (auto I : Indices) {
    if (Composite->getOpCode() == OpUndef ||
        Composite->getOpCode() == OpConstantNull)
      return BM->addNullConstant(ObjectTy);
    else {
      assert(Composite->getOpCode() == OpConstantComposite ||
             Composite->getOpCode() == OpSpecConstantComposite);
      Composite = static_cast<SPIRVConstantComposite *>(
        Composite)->getElements()[I];
    }
  }

  return Composite;
}

SPIRVValue *
constantCompositeInsert(SPIRVValue *Composite, SPIRVValue *Object,
  std::vector<uint32_t> &Indices) {
  SPIRVModule *BM = Composite->getModule();
  SPIRVType *CompositeTy = Composite->getType();
  assert(CompositeTy->isTypeComposite());

  uint32_t ElementCount = CompositeTy->getCompositeElementCount();
  uint32_t Index = Indices[0];
  Indices.erase(Indices.begin());

  std::vector<SPIRVValue *> Elements;
  for (uint32_t I = 0; I < ElementCount; ++I) {
    auto ElementTy = CompositeTy->getCompositeElementType(I);
    SPIRVValue *Element = nullptr;

    if (Composite->getOpCode() == OpUndef ||
        Composite->getOpCode() == OpConstantNull)
      Element = BM->addNullConstant(ElementTy);
    else {
      assert(Composite->getOpCode() == OpConstantComposite ||
             Composite->getOpCode() == OpSpecConstantComposite);
      Element =
        static_cast<SPIRVConstantComposite *>(Composite)->getElements()[I];
    }

    if (I == Index) {
      if (Indices.empty())
        Element = Object; // Encounter the last index
      else
        Element = constantCompositeInsert(Element, Object, Indices);
    }

    assert(Element != nullptr);
    Elements.push_back(Element);
  }

  return BM->addCompositeConstant(CompositeTy, Elements);
}

SPIRVValue *
createValueFromSpecConstantOp(SPIRVSpecConstantOp *Inst) {
  assert(Inst->getOpCode() == OpSpecConstantOp &&
      "Not OpSpecConstantOp");
  auto Ops = Inst->getOpWords();
  auto OC = static_cast<Op>(Ops[0]);
  assert(isSpecConstantOpAllowedOp(OC) &&
      "Op code not allowed for OpSpecConstantOp");
  Ops.erase(Ops.begin(), Ops.begin() + 1);

  auto BM = Inst->getModule();

  uint32_t VOpSize = 0; // Size of value operands (literal operands excluded)
  if (OC == OpVectorShuffle)
    VOpSize = 2;
  else if (OC == OpCompositeExtract)
    VOpSize = 1;
  else if (OC == OpCompositeInsert)
    VOpSize = 2;
  else
    VOpSize = Ops.size();

  for (uint32_t I = 0; I < VOpSize; ++I) {
    auto BV = BM->getValue(Ops[I]);
    if (BV->getOpCode() == OpSpecConstantOp) {
      // NOTE: We have to replace the value created by OpSpecConstantOp with
      // their evaluated (constant folding) values.
      BV = static_cast<SPIRVSpecConstantOp *>(BV)->getMappedConstant();
      Ops[I] = BV->getId();
    }
  }

  union ConstValue {
    bool      BoolVal;
    int32_t   IntVal;
    uint32_t  UintVal;
    int64_t   Int64Val;
    uint64_t  Uint64Val;
    float     FloatVal;
    double    DoubleVal;
  };

  auto DestTy = Inst->getType();

  // Evaluate OpSpecConstantOp by constant folding (new SPIR-V constants might
  // be created in this process, added by addConstant(), addNullConstant(),
  // addCompositeConstant())
  if (OC == OpVectorShuffle) {
    assert(DestTy->isTypeVector());

    const uint32_t CompCount = DestTy->getVectorComponentCount();
    auto DestCompTy = DestTy->getVectorComponentType();

    std::vector<SPIRVValue *> DestComps;

    assert(Ops.size() > 2);
    auto Vec1 = BM->getValue(Ops[0]);
    auto Vec2 = BM->getValue(Ops[1]);
    assert(Vec1->getType()->isTypeVector() && Vec2->getType()->isTypeVector());

    for (uint32_t I = 0; I < CompCount; ++I) {
      ConstValue DestVal = {};
      uint32_t CompSelect = Ops[2 + I];
      if (CompSelect != SPIRVID_INVALID) {
        const uint32_t Vec1CompCount =
          Vec1->getType()->getVectorComponentCount();
        if (CompSelect < Vec1CompCount)
          // Select vector1 as source
          DestVal.Uint64Val = getConstantValue(Vec1, CompSelect);
        else {
          // Select vector2 as source
          CompSelect -= Vec1CompCount;
          DestVal.Uint64Val = getConstantValue(Vec2, CompSelect);
        }
      }

      auto DestComp = BM->addConstant(DestCompTy, DestVal.Uint64Val);
      DestComps.push_back(DestComp);
    }

    return BM->addCompositeConstant(DestTy, DestComps);

  } else if (OC == OpCompositeExtract) {
    assert(Ops.size() >= 2);
    auto ObjectTy = DestTy;
    auto Composite = BM->getValue(Ops[0]);

    std::vector<uint32_t> Indices;
    uint32_t IndexCount = Ops.size() - 1;
    for (uint32_t I = 0; I < IndexCount; ++I)
      Indices.push_back(Ops[1 + I]);

    return constantCompositeExtract(Composite, ObjectTy, Indices);

  } else if (OC == OpCompositeInsert) {
    assert(Ops.size() >= 2);
    auto Object = BM->getValue(Ops[0]);
    auto Composite = BM->getValue(Ops[1]);

    std::vector<uint32_t> Indices;
    uint32_t IndexCount = Ops.size() - 2;
    for (uint32_t I = 0; I < IndexCount; ++I)
      Indices.push_back(Ops[2 + I]);

    return constantCompositeInsert(Composite, Object, Indices);

  } else {
    assert(DestTy->isTypeVector() || DestTy->isTypeScalar());

    const uint32_t CompCount =
      DestTy->isTypeVector() ? DestTy->getVectorComponentCount() : 1;
    auto DestCompTy = (CompCount > 1) ? DestTy->getVectorComponentType() : DestTy;

    auto SrcTy = BM->getValue(Ops[0])->getType();
    auto SrcCompTy = (CompCount > 1) ? SrcTy->getVectorComponentType() : SrcTy;

    std::vector<SPIRVValue *> DestComps;

    for (uint32_t I = 0; I < CompCount; ++I) {
      ConstValue DestVal = {};
      ConstValue SrcVal[3] = {};
      assert(Ops.size() <= 3);

      // Read literal value from source constants
      for (uint32_t J = 0; J < Ops.size(); ++J) {
        auto BV = BM->getValue(Ops[J]);
        if (CompCount == 1)
          SrcVal[J].Uint64Val = getConstantValue(BV);
        else {
          SrcVal[J].Uint64Val = getConstantValue(BV, I);
        }
      }

      // Do computation (constant folding)
      switch (OC) {
      case OpUConvert: {
        if (DestCompTy->isTypeInt(32)) {
          if (SrcCompTy->isTypeInt(64))
            // Uint <- uint64
            DestVal.UintVal = static_cast<uint32_t>(SrcVal[0].Uint64Val);
          else
            llvm_unreachable("Invalid type");
        } else {
          assert(DestCompTy->isTypeInt(64));
          if (SrcCompTy->isTypeInt(32))
            // Uint64 <- uint
            DestVal.Uint64Val = static_cast<uint64_t>(SrcVal[0].UintVal);
          else
            llvm_unreachable("Invalid type");
        }
        break;
      }
      case OpSConvert: {
        if (DestCompTy->isTypeInt(32)) {
          if (SrcCompTy->isTypeInt(64))
            // Int <- int64
            DestVal.IntVal = static_cast<int32_t>(SrcVal[0].Int64Val);
          else
            llvm_unreachable("Invalid type");
        } else {
          assert(DestCompTy->isTypeInt(64));
          if (SrcCompTy->isTypeInt(32))
            // Int64 <- int
            DestVal.Int64Val = static_cast<int64_t>(SrcVal[0].IntVal);
          else
            llvm_unreachable("Invalid type");
        }
        break;
      }
      case OpFConvert: {
        if (DestCompTy->isTypeFloat(32)) {
          if (SrcCompTy->isTypeFloat(64))
            // Float <- double
            DestVal.FloatVal = static_cast<float>(SrcVal[0].DoubleVal);
          else
            llvm_unreachable("Invalid type");
        } else {
          assert(DestCompTy->isTypeFloat(64));
          if (SrcCompTy->isTypeFloat(32))
            // Double <- Float
            DestVal.DoubleVal = static_cast<double>(SrcVal[0].FloatVal);
          else
            llvm_unreachable("Invalid type");
        }
        break;
      }
      case OpQuantizeToF16: {
        spvutils::HexFloat<spvutils::FloatProxy<float>> FVal(SrcVal[0].FloatVal);
        spvutils::HexFloat<spvutils::FloatProxy<spvutils::Float16>> F16Val(0);

        FVal.castTo(F16Val, spvutils::kRoundToZero);
        // Flush denormals to zero (sign is preserved)
        if (F16Val.getExponentBits() == 0 && F16Val.getSignificandBits() != 0)
            F16Val.set_value(F16Val.isNegative() ? F16Val.sign_mask : 0);
        F16Val.castTo(FVal, spvutils::kRoundToZero);
        DestVal.FloatVal = FVal.value().getAsFloat();

        break;
      }
      case OpSNegate: {
        if (DestCompTy->isTypeInt(32))
          // -Int
          DestVal.IntVal = (0 - SrcVal[0].IntVal);
        else {
          // -Int64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = (0 - SrcVal[0].Int64Val);
        }
        break;
      }
      case OpNot: {
        if (DestCompTy->isTypeInt(32))
          // ~Uint
          DestVal.UintVal = ~SrcVal[0].UintVal;
        else {
          // ~Uint64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = ~SrcVal[0].Uint64Val;
        }
        break;
      }
      case OpIAdd: {
        if (DestCompTy->isTypeInt(32))
          // Int + int
          DestVal.IntVal = SrcVal[0].IntVal + SrcVal[1].IntVal;
        else {
          // Int64 + int64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = SrcVal[0].Int64Val + SrcVal[1].Int64Val;
        }
        break;
      }
      case OpISub: {
        if (DestCompTy->isTypeInt(32))
          // Int - int
          DestVal.IntVal = SrcVal[0].IntVal - SrcVal[1].IntVal;
        else {
          // Int64 - int64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = SrcVal[0].Int64Val - SrcVal[1].Int64Val;
        }
        break;
      }
      case OpIMul: {
        if (DestCompTy->isTypeInt(32))
          // Int * int
          DestVal.IntVal = SrcVal[0].IntVal * SrcVal[1].IntVal;
        else {
          // Int64 * int64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = SrcVal[0].Int64Val * SrcVal[1].Int64Val;
        }
        break;
      }
      case OpUDiv: {
        if (DestCompTy->isTypeInt(32))
          // Uint / uint
          DestVal.UintVal = SrcVal[0].UintVal / SrcVal[1].UintVal;
        else {
          // Uint64 / uint64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val / SrcVal[1].Uint64Val;
        }
        break;
      }
      case OpSDiv: {
        if (DestCompTy->isTypeInt(32))
          // Int / int
          DestVal.IntVal = SrcVal[0].IntVal / SrcVal[1].IntVal;
        else {
          // Int64 / int64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = SrcVal[0].Int64Val / SrcVal[1].Int64Val;
        }
        break;
      }
      case OpUMod: {
        if (DestCompTy->isTypeInt(32))
          // Uint % uint
          DestVal.UintVal = SrcVal[0].UintVal % SrcVal[1].UintVal;
        else {
          // Uint64 % uint64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val % SrcVal[1].Uint64Val;
        }
        break;
      }
      case OpSMod: {
        if (DestCompTy->isTypeInt(32)) {
          // Mod(int, int)
          float Quo =
            static_cast<float>(SrcVal[0].IntVal) / SrcVal[1].IntVal;
          DestVal.IntVal = SrcVal[0].IntVal -
            SrcVal[1].IntVal * static_cast<int32_t>(std::floor(Quo));
        } else {
          // Mod(int64, int64)
          assert(DestCompTy->isTypeInt(64));
          double Quo =
            static_cast<double>(SrcVal[0].Int64Val) / SrcVal[1].Int64Val;
          DestVal.Int64Val = SrcVal[0].Int64Val -
            SrcVal[1].Int64Val * static_cast<int64_t>(std::floor(Quo));
        }
        break;
      }
      case OpSRem: {
        if (DestCompTy->isTypeInt(32))
          // Int % int
          DestVal.IntVal = SrcVal[0].IntVal % SrcVal[1].IntVal;
        else {
          // Int64 % int64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = SrcVal[0].Int64Val % SrcVal[1].Int64Val;
        }
        break;
      }
      case OpShiftRightLogical: {
        // NOTE: "Shift number" is consumed as an 32-bit unsigned integer
        // regardless of its actual type.
        if (DestCompTy->isTypeInt(32))
          // Uint >> uint
          DestVal.UintVal = SrcVal[0].UintVal >> SrcVal[1].UintVal;
        else {
          // Uint64 >> uint
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val >> SrcVal[1].UintVal;
        }
        break;
      }
      case OpShiftRightArithmetic: {
        // NOTE: "Shift number" is consumed as an 32-bit unsigned integer
        // regardless of its actual type.
        if (DestCompTy->isTypeInt(32))
          // Int >> uint
          DestVal.IntVal = SrcVal[0].IntVal >> SrcVal[1].UintVal;
        else {
          // Int64 >> uint
          assert(DestCompTy->isTypeInt(64));
          DestVal.Int64Val = SrcVal[0].Int64Val >> SrcVal[1].UintVal;
        }
        break;
      }
      case OpShiftLeftLogical: {
        // NOTE: "Shift number" is consumed as an 32-bit unsigned integer
        // regardless of its actual type.
        if (DestCompTy->isTypeInt(32))
          // Uint << uint
          DestVal.UintVal = SrcVal[0].UintVal << SrcVal[1].UintVal;
        else {
          // Uint64 << uint
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val << SrcVal[1].UintVal;
        }
        break;
      }
      case OpBitwiseOr: {
        if (DestCompTy->isTypeInt(32))
          // Uint | uint
          DestVal.UintVal = SrcVal[0].UintVal | SrcVal[1].UintVal;
        else {
          // Uint64 | uint64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val | SrcVal[1].Uint64Val;
        }
        break;
      }
      case OpBitwiseXor: {
        if (DestCompTy->isTypeInt(32))
          // Uint ^ uint
          DestVal.UintVal = SrcVal[0].UintVal ^ SrcVal[1].UintVal;
        else {
          // Uint64 ^ uint64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val ^ SrcVal[1].Uint64Val;
        }
        break;
      }
      case OpBitwiseAnd: {
        if (DestCompTy->isTypeInt(32))
          // Uint & uint
          DestVal.UintVal = SrcVal[0].UintVal & SrcVal[1].UintVal;
        else {
          // Uint64 & uint64
          assert(DestCompTy->isTypeInt(64));
          DestVal.Uint64Val = SrcVal[0].Uint64Val & SrcVal[1].Uint64Val;
        }
        break;
      }
      case OpLogicalOr:
        // Bool || bool
        DestVal.BoolVal = (SrcVal[0].BoolVal || SrcVal[1].BoolVal);
        break;
      case OpLogicalAnd:
        // Bool && bool
        DestVal.BoolVal = (SrcVal[0].BoolVal && SrcVal[1].BoolVal);
        break;
      case OpLogicalNot:
        // ! Bool
        DestVal.BoolVal = !SrcVal[0].BoolVal;
        break;
      case OpLogicalEqual:
        // Bool == bool
        DestVal.BoolVal = (SrcVal[0].BoolVal == SrcVal[1].BoolVal);
        break;
      case OpLogicalNotEqual:
        // Bool != bool
        DestVal.BoolVal = (SrcVal[0].BoolVal != SrcVal[1].BoolVal);
        break;
      case OpSelect:
        // Bool ? value : value
        DestVal = SrcVal[0].BoolVal ? SrcVal[1] : SrcVal[2];
        break;
      case OpIEqual: {
        if (SrcCompTy->isTypeInt(32))
          // Uint == uint
          DestVal.BoolVal = (SrcVal[0].UintVal == SrcVal[1].UintVal);
        else {
          // Uint64 == uint64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Uint64Val == SrcVal[1].Uint64Val);
        }
        break;
      }
      case OpINotEqual: {
        if (SrcCompTy->isTypeInt(32))
          // Uint != uint
          DestVal.BoolVal = (SrcVal[0].UintVal != SrcVal[1].UintVal);
        else {
          // Uint64 != uint64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Uint64Val != SrcVal[1].Uint64Val);
        }
        break;
      }
      case OpULessThan: {
        if (SrcCompTy->isTypeInt(32))
          // Uint < uint
          DestVal.BoolVal = (SrcVal[0].UintVal < SrcVal[1].UintVal);
        else {
          // Uint64 < uint64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Uint64Val < SrcVal[1].Uint64Val);
        }
        break;
      }
      case OpSLessThan: {
        if (SrcCompTy->isTypeInt(32))
          // Int < int
          DestVal.BoolVal = (SrcVal[0].IntVal < SrcVal[1].IntVal);
        else {
          // Int64 < int64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Int64Val < SrcVal[1].Int64Val);
        }
        break;
      }
      case OpUGreaterThan: {
        if (SrcCompTy->isTypeInt(32))
          // Uint > uint
          DestVal.BoolVal = (SrcVal[0].UintVal > SrcVal[1].UintVal);
        else {
          // Uint64 > uint64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Uint64Val > SrcVal[1].Uint64Val);
        }
        break;
      }
      case OpSGreaterThan: {
        if (SrcCompTy->isTypeInt(32))
          // Int > int
          DestVal.BoolVal = (SrcVal[0].IntVal > SrcVal[1].IntVal);
        else {
          // Int64 > int64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Int64Val > SrcVal[1].Int64Val);
        }
        break;
      }
      case OpULessThanEqual: {
        if (SrcCompTy->isTypeInt(32))
          // Uint <= uint
          DestVal.BoolVal = (SrcVal[0].UintVal <= SrcVal[1].UintVal);
        else {
          // Uint64 <= uint64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Uint64Val <= SrcVal[1].Uint64Val);
        }
        break;
      }
      case OpSLessThanEqual: {
        if (SrcCompTy->isTypeInt(32))
          // Int <= int
          DestVal.BoolVal = (SrcVal[0].IntVal <= SrcVal[1].IntVal);
        else {
          // Int64 <= int64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Int64Val <= SrcVal[1].Int64Val);
        }
        break;
      }
      case OpUGreaterThanEqual: {
        if (SrcCompTy->isTypeInt(32))
          // Uint >= uint
          DestVal.BoolVal = (SrcVal[0].UintVal >= SrcVal[1].UintVal);
        else {
          // Uint64 >= uint64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Uint64Val >= SrcVal[1].Uint64Val);
        }
        break;
      }
      case OpSGreaterThanEqual: {
        if (SrcCompTy->isTypeInt(32))
          // Int >= int
          DestVal.BoolVal = (SrcVal[0].IntVal >= SrcVal[1].IntVal);
        else {
          // Int64 >= int64
          assert(SrcCompTy->isTypeInt(64));
          DestVal.BoolVal = (SrcVal[0].Int64Val >= SrcVal[1].Int64Val);
        }
        break;
      }
      default:
        llvm_unreachable("Op code only allowed for OpenCL kernel");
        break;
      }

      // Write literal value to destination constant
      auto DestComp = BM->addConstant(DestCompTy, DestVal.Uint64Val);
      DestComps.push_back(DestComp);
    }

    if (CompCount == 1)
      return DestComps[0];
    else
      return BM->addCompositeConstant(DestTy, DestComps);
  }
}

}


