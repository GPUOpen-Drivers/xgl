/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
 ***********************************************************************************************************************
 * @file  llpcPatchAddrSpaceMutate.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchAddrSpaceMutate.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-addr-space-mutate"

#include "llvm/ADT/None.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"

#include <vector>
#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcIntrinsDefs.h"
#include "llpcPatchAddrSpaceMutate.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchAddrSpaceMutate::ID = 0;

// =====================================================================================================================
PatchAddrSpaceMutate::PatchAddrSpaceMutate()
    :
    Patch(ID)
{
    initializePatchAddrSpaceMutatePass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this patching pass on the specified LLVM module.
//
// This pass converts SPIR-V address spaces to target machine address spaces, and sets the triple and data layout.
bool PatchAddrSpaceMutate::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-Addr-Space-Mutate\n");

    Patch::Init(&module);

    // Build a mapping from SPIR-V address space to target address space.
    m_addrSpaceMap.resize(SPIRAS_Count);

    auto pTargetMachine = m_pContext->GetTargetMachine();
    auto dataLayout = pTargetMachine->createDataLayout();

    m_addrSpaceMap[SPIRAS_Private] = dataLayout.getAllocaAddrSpace();
    m_addrSpaceMap[SPIRAS_Global] = ADDR_SPACE_GLOBAL;
    m_addrSpaceMap[SPIRAS_Constant] = ADDR_SPACE_CONST;
    m_addrSpaceMap[SPIRAS_Local] = ADDR_SPACE_LOCAL;

    // Gather the globals and then process them. We do not want to reprocess globals that we create
    // here. Ignore unused globals left behind by lowering passes.
    SmallVector<GlobalVariable*, 8> globalVars;
    for (auto globalIt = module.global_begin(), globalItEnd = module.global_end();
          globalIt != globalItEnd; ++globalIt)
    {
        auto pGlobalVar = dyn_cast<GlobalVariable>(&*globalIt);
        if ((pGlobalVar != nullptr) && (pGlobalVar->use_empty() == false))
        {
            globalVars.push_back(pGlobalVar);
        }
    }

    // For any global variable whose type needs to change, create a new one. We only cope with the
    // case where the top level address space changes, so we do not need to worry about modifying
    // any initializer.
    for (uint32_t globalVarIdx = 0; globalVarIdx != globalVars.size(); ++globalVarIdx)
    {
        auto pOldGlobalVar = globalVars[globalVarIdx];
        auto pOldGlobalVarType = cast<PointerType>(pOldGlobalVar->getType());
        auto pNewGlobalVarType = cast<PointerType>(MapType(pOldGlobalVarType));

        if (pOldGlobalVarType != pNewGlobalVarType)
        {
            LLPC_ASSERT(pOldGlobalVarType->getElementType() == pNewGlobalVarType->getElementType());

            auto pNewGlobalVar = new GlobalVariable(module,
                                                    pOldGlobalVarType->getElementType(),
                                                    pOldGlobalVar->isConstant(),
                                                    pOldGlobalVar->getLinkage(),
                                                    pOldGlobalVar->hasInitializer() ?
                                                        pOldGlobalVar->getInitializer() : nullptr,
                                                    "",
                                                    nullptr,
                                                    pOldGlobalVar->getThreadLocalMode(),
                                                    pNewGlobalVarType->getAddressSpace(),
                                                    pOldGlobalVar->isExternallyInitialized());

            pNewGlobalVar->takeName(pOldGlobalVar);
            m_globalMap[pOldGlobalVar] = pNewGlobalVar;
        }
    }

    // Gather the functions and then process them. We do not want to reprocess functions that we create here.
    SmallVector<Function*, 8> funcs;
    for (auto funcIt = module.begin(), funcItEnd = module.end(); funcIt != funcItEnd; ++funcIt)
    {
        funcs.push_back(&*funcIt);
    }

    // For any function where the type needs to change, create a new function.
    for (uint32_t funcIndex = 0; funcIndex != funcs.size(); ++funcIndex)
    {
        auto pOldFunc = funcs[funcIndex];
        auto pOldFuncType = pOldFunc->getFunctionType();
        auto pNewFuncType = cast<FunctionType>(MapType(pOldFuncType));
        if (pOldFuncType != pNewFuncType)
        {
            // TODO: We would need to handle a function _definition_ (one with a body) here if we stop
            // inlining everything in LLPC.
            LLPC_ASSERT(pOldFunc->empty());

            // Create a new function with the modified type. We need to supply the name upfront, rather than
            // use takeName() afterwards, so that the intrinsic ID gets set correctly.
            std::string funcName = pOldFunc->getName().str();
            pOldFunc->setName("");
            auto pNewFunc = Function::Create(pNewFuncType, pOldFunc->getLinkage(), funcName, &module);
            pNewFunc->copyAttributesFrom(pOldFunc);
            pNewFunc->copyMetadata(pOldFunc, 0);

            // If this is an intrinsic, remangle the name.
            auto remangledFunc = Intrinsic::remangleIntrinsicFunction(pNewFunc);
            if (remangledFunc != None)
            {
                pNewFunc->eraseFromParent();
                pNewFunc = remangledFunc.getValue();
            }

            // Add to the map for call instructions to reference.
            m_globalMap[pOldFunc] = pNewFunc;
        }
    }

    // Process instructions in functions.
    for (unsigned funcIndex = 0; funcIndex != funcs.size(); ++funcIndex)
    {
        auto pFunc = funcs[funcIndex];
        if (pFunc->empty() == false)
        {
            ProcessFunction(pFunc);
        }
    }

    // Remove any global that we replaced with a different type one.
    for (auto globalMapIt = m_globalMap.begin(), globalMapItEnd = m_globalMap.end();
          globalMapIt != globalMapItEnd; ++globalMapIt)
    {
        globalMapIt->first->eraseFromParent();
    }

    // Change the triple and data layout.
    m_pContext->SetModuleTargetMachine(&module);

    m_typeMap.clear();
    m_globalMap.clear();

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Processes the specified function by mutating types and global references as necessary in instructions in the function.
void PatchAddrSpaceMutate::ProcessFunction(
    Function* pFunc)  // [in] Function to be processed
{
    for (auto blockIt = pFunc->begin(), blockItEnd = pFunc->end(); blockIt != blockItEnd; ++blockIt)
    {
        BasicBlock* pBlock = &*blockIt;
        for (auto instIt = pBlock->begin(), instItEnd = pBlock->end(); instIt != instItEnd; ++instIt)
        {
            Instruction* pInst = &*instIt;
            // For each instruction...
            // First change the type of any pointer constant operand. Only two cases need handling:
            // 1. global pointer
            // 2. nullptr.
            for (uint32_t operandIdx = 0, operandCount = pInst->getNumOperands();
                  operandIdx != operandCount; ++operandIdx)
            {
                auto pGlobal = dyn_cast<GlobalValue>(pInst->getOperand(operandIdx));
                if (pGlobal != nullptr)
                {
                    auto globalMapIt = m_globalMap.find(pGlobal);
                    if (globalMapIt != m_globalMap.end())
                    {
                        pInst->setOperand(operandIdx, globalMapIt->second);
                    }
                }
                else
                {
                    auto pConst = dyn_cast<Constant>(pInst->getOperand(operandIdx));
                    if (pConst != nullptr)
                    {
                        if (auto pOldType = dyn_cast<PointerType>(pConst->getType()))
                        {
                            auto pNewType = cast<PointerType>(MapType(pOldType));
                            if (pOldType != pNewType)
                            {
                                LLPC_ASSERT(isa<ConstantPointerNull>(pConst));
                                pInst->setOperand(operandIdx, ConstantPointerNull::get(pNewType));
                            }
                        }
                    }
                }
            }
            // Then change the type of the result.
            auto pCall = dyn_cast<CallInst>(pInst);
            if (pCall != nullptr)
            {
                pCall->mutateFunctionType(cast<FunctionType>(MapType(pCall->getFunctionType())));
            }
            else
            {
                pInst->mutateType(MapType(pInst->getType()));
            }
        }
    }
}

// =====================================================================================================================
// Maps a pointer type to the equivalent with a modified address space.
Type* PatchAddrSpaceMutate::MapType(
    Type* pOldType) // [in] Old type to be mapped
{
    auto ppNewType = &m_typeMap[pOldType];
    if (*ppNewType == nullptr)
    {
        *ppNewType = pOldType;
        auto pOldPtrType = dyn_cast<PointerType>(pOldType);
        if (pOldPtrType != nullptr)
        {
            // For a pointer, map the element type.
            auto pOldElemType = pOldPtrType->getElementType();
            auto pNewElemType = MapType(pOldElemType);

            // For a non-function pointer, map the address space.
            auto oldAddrSpace = pOldPtrType->getAddressSpace();
            auto newAddrSpace = oldAddrSpace;
            if (isa<FunctionType>(pOldElemType) == false)
            {
                newAddrSpace = m_addrSpaceMap[oldAddrSpace];
            }

            // If the element type or the address space needs to change, get a new pointer type.
            if ((oldAddrSpace != newAddrSpace) || (pOldElemType != pNewElemType))
            {
                *ppNewType = PointerType::get(pNewElemType, newAddrSpace);
            }
        }
        else
        {
            auto pOldFuncType = dyn_cast<FunctionType>(pOldType);
            if (pOldFuncType != nullptr)
            {
                // For a function type, map the return and parameter types.
                LLPC_ASSERT(!pOldFuncType->isVarArg());
                auto pOldRetType = pOldFuncType->getReturnType();
                auto pNewRetType = MapType(pOldRetType);
                bool isChanged = (pOldRetType != pNewRetType);

                SmallVector<Type*, 4> newParamTypes;
                for (uint32_t paramIndex = 0, paramCount = pOldFuncType->getNumParams();
                        paramIndex != paramCount; ++paramIndex)
                {
                    auto pOldParamType = pOldFuncType->getParamType(paramIndex);
                    newParamTypes.push_back(MapType(pOldParamType));
                    isChanged |= (pOldParamType != newParamTypes.back());
                }

                if (isChanged)
                {
                    *ppNewType = FunctionType::get(pNewRetType, newParamTypes, false);
                }
            }
        }
        // TODO: We only do address space mutation for pointer and function types. To be completely general,
        // any aggregate type that contains pointer type should be taken into account. Currently, we have not
        // encountered such case yet.
    }
    return *ppNewType;
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patching operations of mutate address spaces from SPIRAS to AMDGPU.
INITIALIZE_PASS(PatchAddrSpaceMutate, "Patch-addr-space-mutate",
                "Patch LLVM for addr space mutation (from SPIRAS to AMDGPU)", false, false)

