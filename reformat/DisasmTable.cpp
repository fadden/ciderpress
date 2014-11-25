/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Tables used in 65xxx disassembly.
 */
#include "StdAfx.h"
#include "Disasm.h"

/*
 * These values make the table a little easier to read, and prevent some
 * varieties of typographical error.  As C++ consts they should not occupy
 * any space in the object code.
 */
#define OPMODE(op, addr) (ReformatDisasm65xxx::op | ReformatDisasm65xxx::addr << 8)

const int kOpUnknown                = OPMODE(kOpCodeUnknown, kAddrModeUnknown);

const int kOpADC_Imm                = OPMODE(kOpADC, kAddrImm);             // 0x69
const int kOpADC_Abs                = OPMODE(kOpADC, kAddrAbs);             // 0x6d
const int kOpADC_AbsLong            = OPMODE(kOpADC, kAddrAbsLong);         // 0x6f
const int kOpADC_DP                 = OPMODE(kOpADC, kAddrDP);              // 0x65
const int kOpADC_DPInd              = OPMODE(kOpADC, kAddrDPInd);           // 0x72
const int kOpADC_DPIndLong          = OPMODE(kOpADC, kAddrDPIndLong);       // 0x67
const int kOpADC_AbsIndexX          = OPMODE(kOpADC, kAddrAbsIndexX);       // 0x7d
const int kOpADC_AbsIndexXLong      = OPMODE(kOpADC, kAddrAbsIndexXLong);   // 0x7f
const int kOpADC_AbsIndexY          = OPMODE(kOpADC, kAddrAbsIndexY);       // 0x79
const int kOpADC_DPIndexX           = OPMODE(kOpADC, kAddrDPIndexX);        // 0x75
const int kOpADC_DPIndexXInd        = OPMODE(kOpADC, kAddrDPIndexXInd);     // 0x61
const int kOpADC_DPIndIndexY        = OPMODE(kOpADC, kAddrDPIndIndexY);     // 0x71
const int kOpADC_DPIndIndexYLong    = OPMODE(kOpADC, kAddrDPIndIndexYLong); // 0x77
const int kOpADC_StackRel           = OPMODE(kOpADC, kAddrStackRel);        // 0x63
const int kOpADC_StackRelIndexY     = OPMODE(kOpADC, kAddrStackRelIndexY);  // 0x73

const int kOpAND_Imm                = OPMODE(kOpAND, kAddrImm);             // 0x29
const int kOpAND_Abs                = OPMODE(kOpAND, kAddrAbs);             // 0x2d
const int kOpAND_AbsLong            = OPMODE(kOpAND, kAddrAbsLong);         // 0x2f
const int kOpAND_DP                 = OPMODE(kOpAND, kAddrDP);              // 0x25
const int kOpAND_DPInd              = OPMODE(kOpAND, kAddrDPInd);           // 0x32
const int kOpAND_DPIndLong          = OPMODE(kOpAND, kAddrDPIndLong);       // 0x27
const int kOpAND_AbsIndexX          = OPMODE(kOpAND, kAddrAbsIndexX);       // 0x3d
const int kOpAND_AbsIndexXLong      = OPMODE(kOpAND, kAddrAbsIndexXLong);   // 0x3f
const int kOpAND_AbsIndexY          = OPMODE(kOpAND, kAddrAbsIndexY);       // 0x39
const int kOpAND_DPIndexX           = OPMODE(kOpAND, kAddrDPIndexX);        // 0x35
const int kOpAND_DPIndexXInd        = OPMODE(kOpAND, kAddrDPIndexXInd);     // 0x21
const int kOpAND_DPIndIndexY        = OPMODE(kOpAND, kAddrDPIndIndexY);     // 0x31
const int kOpAND_DPIndIndexYLong    = OPMODE(kOpAND, kAddrDPIndIndexYLong); // 0x37
const int kOpAND_StackRel           = OPMODE(kOpAND, kAddrStackRel);        // 0x23
const int kOpAND_StackRelIndexY     = OPMODE(kOpAND, kAddrStackRelIndexY);  // 0x33

const int kOpASL_Acc                = OPMODE(kOpASL, kAddrAcc);             // 0x0a
const int kOpASL_Abs                = OPMODE(kOpASL, kAddrAbs);             // 0x0e
const int kOpASL_DP                 = OPMODE(kOpASL, kAddrDP);              // 0x06
const int kOpASL_AbsIndexX          = OPMODE(kOpASL, kAddrAbsIndexX);       // 0x1e
const int kOpASL_DPIndexX           = OPMODE(kOpASL, kAddrDPIndexX);        // 0x16

const int kOpBCC_PCRel              = OPMODE(kOpBCC, kAddrPCRel);           // 0x90
const int kOpBCS_PCRel              = OPMODE(kOpBCS, kAddrPCRel);           // 0xb0
const int kOpBEQ_PCRel              = OPMODE(kOpBEQ, kAddrPCRel);           // 0xf0

const int kOpBIT_Imm                = OPMODE(kOpBIT, kAddrImm);             // 0x89
const int kOpBIT_Abs                = OPMODE(kOpBIT, kAddrAbs);             // 0x2c
const int kOpBIT_DP                 = OPMODE(kOpBIT, kAddrDP);              // 0x24
const int kOpBIT_AbsIndexX          = OPMODE(kOpBIT, kAddrAbsIndexX);       // 0x3c
const int kOpBIT_DPIndexX           = OPMODE(kOpBIT, kAddrDPIndexX);        // 0x34

const int kOpBMI_PCRel              = OPMODE(kOpBMI, kAddrPCRel);           // 0x30
const int kOpBNE_PCRel              = OPMODE(kOpBNE, kAddrPCRel);           // 0xd0
const int kOpBPL_PCRel              = OPMODE(kOpBPL, kAddrPCRel);           // 0x10
const int kOpBRA_PCRel              = OPMODE(kOpBRA, kAddrPCRel);           // 0x80

const int kOpBRK_StackInt           = OPMODE(kOpBRK, kAddrStackInt);        // 0x00

const int kOpBRL_PCRelLong          = OPMODE(kOpBRL, kAddrPCRelLong);       // 0x82
const int kOpBVC_PCRel              = OPMODE(kOpBVC, kAddrPCRel);           // 0x50
const int kOpBVS_PCRel              = OPMODE(kOpBVS, kAddrPCRel);           // 0x70

const int kOpCLC_Implied            = OPMODE(kOpCLC, kAddrImplied);         // 0x18
const int kOpCLD_Implied            = OPMODE(kOpCLD, kAddrImplied);         // 0xd8
const int kOpCLI_Implied            = OPMODE(kOpCLI, kAddrImplied);         // 0x58
const int kOpCLV_Implied            = OPMODE(kOpCLV, kAddrImplied);         // 0xb8

const int kOpCMP_Imm                = OPMODE(kOpCMP, kAddrImm);             // 0xc9
const int kOpCMP_Abs                = OPMODE(kOpCMP, kAddrAbs);             // 0xcd
const int kOpCMP_AbsLong            = OPMODE(kOpCMP, kAddrAbsLong);         // 0xcf
const int kOpCMP_DP                 = OPMODE(kOpCMP, kAddrDP);              // 0xc5
const int kOpCMP_DPInd              = OPMODE(kOpCMP, kAddrDPInd);           // 0xd2
const int kOpCMP_DPIndLong          = OPMODE(kOpCMP, kAddrDPIndLong);       // 0xc7
const int kOpCMP_AbsIndexX          = OPMODE(kOpCMP, kAddrAbsIndexX);       // 0xdd
const int kOpCMP_AbsIndexXLong      = OPMODE(kOpCMP, kAddrAbsIndexXLong);   // 0xdf
const int kOpCMP_AbsIndexY          = OPMODE(kOpCMP, kAddrAbsIndexY);       // 0xd9
const int kOpCMP_DPIndexX           = OPMODE(kOpCMP, kAddrDPIndexX);        // 0xd5
const int kOpCMP_DPIndexXInd        = OPMODE(kOpCMP, kAddrDPIndexXInd);     // 0xc1
const int kOpCMP_DPIndIndexY        = OPMODE(kOpCMP, kAddrDPIndIndexY);     // 0xd1
const int kOpCMP_DPIndIndexYLong    = OPMODE(kOpCMP, kAddrDPIndIndexYLong); // 0xd7
const int kOpCMP_StackRel           = OPMODE(kOpCMP, kAddrStackRel);        // 0xc3
const int kOpCMP_StackRelIndexY     = OPMODE(kOpCMP, kAddrStackRelIndexY);  // 0xd3

const int kOpCOP_StackInt           = OPMODE(kOpCOP, kAddrStackInt);        // 0x02

const int kOpCPX_Imm                = OPMODE(kOpCPX, kAddrImm);             // 0xe0
const int kOpCPX_Abs                = OPMODE(kOpCPX, kAddrAbs);             // 0xec
const int kOpCPX_DP                 = OPMODE(kOpCPX, kAddrDP);              // 0xe4

const int kOpCPY_Imm                = OPMODE(kOpCPY, kAddrImm);             // 0xc0
const int kOpCPY_Abs                = OPMODE(kOpCPY, kAddrAbs);             // 0xcc
const int kOpCPY_DP                 = OPMODE(kOpCPY, kAddrDP);              // 0xc4

const int kOpDEC_Acc                = OPMODE(kOpDEC, kAddrAcc);             // 0x3a
const int kOpDEC_Abs                = OPMODE(kOpDEC, kAddrAbs);             // 0xce
const int kOpDEC_DP                 = OPMODE(kOpDEC, kAddrDP);              // 0xc6
const int kOpDEC_AbsIndexX          = OPMODE(kOpDEC, kAddrAbsIndexX);       // 0xde
const int kOpDEC_DPIndexX           = OPMODE(kOpDEC, kAddrDPIndexX);        // 0xd6

const int kOpDEX_Implied            = OPMODE(kOpDEX, kAddrImplied);         // 0xca
const int kOpDEY_Implied            = OPMODE(kOpDEY, kAddrImplied);         // 0x88

const int kOpEOR_Imm                = OPMODE(kOpEOR, kAddrImm);             // 0x49
const int kOpEOR_Abs                = OPMODE(kOpEOR, kAddrAbs);             // 0x4d
const int kOpEOR_AbsLong            = OPMODE(kOpEOR, kAddrAbsLong);         // 0x4f
const int kOpEOR_DP                 = OPMODE(kOpEOR, kAddrDP);              // 0x45
const int kOpEOR_DPInd              = OPMODE(kOpEOR, kAddrDPInd);           // 0x52
const int kOpEOR_DPIndLong          = OPMODE(kOpEOR, kAddrDPIndLong);       // 0x47
const int kOpEOR_AbsIndexX          = OPMODE(kOpEOR, kAddrAbsIndexX);       // 0x5d
const int kOpEOR_AbsIndexXLong      = OPMODE(kOpEOR, kAddrAbsIndexXLong);   // 0x5f
const int kOpEOR_AbsIndexY          = OPMODE(kOpEOR, kAddrAbsIndexY);       // 0x59
const int kOpEOR_DPIndexX           = OPMODE(kOpEOR, kAddrDPIndexX);        // 0x55
const int kOpEOR_DPIndexXInd        = OPMODE(kOpEOR, kAddrDPIndexXInd);     // 0x41
const int kOpEOR_DPIndIndexY        = OPMODE(kOpEOR, kAddrDPIndIndexY);     // 0x51
const int kOpEOR_DPIndIndexYLong    = OPMODE(kOpEOR, kAddrDPIndIndexYLong); // 0x57
const int kOpEOR_StackRel           = OPMODE(kOpEOR, kAddrStackRel);        // 0x43
const int kOpEOR_StackRelIndexY     = OPMODE(kOpEOR, kAddrStackRelIndexY);  // 0x53

const int kOpINC_Acc                = OPMODE(kOpINC, kAddrAcc);             // 0x1a
const int kOpINC_Abs                = OPMODE(kOpINC, kAddrAbs);             // 0xee
const int kOpINC_DP                 = OPMODE(kOpINC, kAddrDP);              // 0xe6
const int kOpINC_AbsIndexX          = OPMODE(kOpINC, kAddrAbsIndexX);       // 0xfe
const int kOpINC_DPIndexX           = OPMODE(kOpINC, kAddrDPIndexX);        // 0xf6

const int kOpINX_Implied            = OPMODE(kOpINX, kAddrImplied);         // 0xe8
const int kOpINY_Implied            = OPMODE(kOpINY, kAddrImplied);         // 0xc8

const int kOpJML_AbsIndLong         = OPMODE(kOpJML, kAddrAbsIndLong);      // 0xdc

const int kOpJMP_Abs                = OPMODE(kOpJMP, kAddrAbs);             // 0x4c
const int kOpJMP_AbsInd             = OPMODE(kOpJMP, kAddrAbsInd);          // 0x6c
const int kOpJMP_AbsIndexXInd       = OPMODE(kOpJMP, kAddrAbsIndexXInd);    // 0x7c
const int kOpJMP_AbsLong            = OPMODE(kOpJMP, kAddrAbsLong);         // 0x5c

const int kOpJSL_AbsLong            = OPMODE(kOpJSL, kAddrAbsLong);         // 0x22

const int kOpJSR_Abs                = OPMODE(kOpJSR, kAddrAbs);             // 0x20
const int kOpJSR_AbsIndexXInd       = OPMODE(kOpJSR, kAddrAbsIndexXInd);    // 0xfc

const int kOpLDA_Imm                = OPMODE(kOpLDA, kAddrImm);             // 0xa9
const int kOpLDA_Abs                = OPMODE(kOpLDA, kAddrAbs);             // 0xad
const int kOpLDA_AbsLong            = OPMODE(kOpLDA, kAddrAbsLong);         // 0xaf
const int kOpLDA_DP                 = OPMODE(kOpLDA, kAddrDP);              // 0xa5
const int kOpLDA_DPInd              = OPMODE(kOpLDA, kAddrDPInd);           // 0xb2
const int kOpLDA_DPIndLong          = OPMODE(kOpLDA, kAddrDPIndLong);       // 0xa7
const int kOpLDA_AbsIndexX          = OPMODE(kOpLDA, kAddrAbsIndexX);       // 0xbd
const int kOpLDA_AbsIndexXLong      = OPMODE(kOpLDA, kAddrAbsIndexXLong);   // 0xbf
const int kOpLDA_AbsIndexY          = OPMODE(kOpLDA, kAddrAbsIndexY);       // 0xb9
const int kOpLDA_DPIndexX           = OPMODE(kOpLDA, kAddrDPIndexX);        // 0xb5
const int kOpLDA_DPIndexXInd        = OPMODE(kOpLDA, kAddrDPIndexXInd);     // 0xa1
const int kOpLDA_DPIndIndexY        = OPMODE(kOpLDA, kAddrDPIndIndexY);     // 0xb1
const int kOpLDA_DPIndIndexYLong    = OPMODE(kOpLDA, kAddrDPIndIndexYLong); // 0xb7
const int kOpLDA_StackRel           = OPMODE(kOpLDA, kAddrStackRel);        // 0xa3
const int kOpLDA_StackRelIndexY     = OPMODE(kOpLDA, kAddrStackRelIndexY);  // 0xb3

const int kOpLDX_Imm                = OPMODE(kOpLDX, kAddrImm);             // 0xa2
const int kOpLDX_Abs                = OPMODE(kOpLDX, kAddrAbs);             // 0xae
const int kOpLDX_DP                 = OPMODE(kOpLDX, kAddrDP);              // 0xa6
const int kOpLDX_AbsIndexY          = OPMODE(kOpLDX, kAddrAbsIndexY);       // 0xbe
const int kOpLDX_DPIndexY           = OPMODE(kOpLDX, kAddrDPIndexY);        // 0xb6

const int kOpLDY_Imm                = OPMODE(kOpLDY, kAddrImm);             // 0xa0
const int kOpLDY_Abs                = OPMODE(kOpLDY, kAddrAbs);             // 0xac
const int kOpLDY_DP                 = OPMODE(kOpLDY, kAddrDP);              // 0xa4
const int kOpLDY_AbsIndexX          = OPMODE(kOpLDY, kAddrAbsIndexX);       // 0xbc
const int kOpLDY_DPIndexX           = OPMODE(kOpLDY, kAddrDPIndexX);        // 0xb4

const int kOpLSR_Acc                = OPMODE(kOpLSR, kAddrAcc);             // 0x4a
const int kOpLSR_Abs                = OPMODE(kOpLSR, kAddrAbs);             // 0x4e
const int kOpLSR_DP                 = OPMODE(kOpLSR, kAddrDP);              // 0x46
const int kOpLSR_AbsIndexX          = OPMODE(kOpLSR, kAddrAbsIndexX);       // 0x5e
const int kOpLSR_DPIndexX           = OPMODE(kOpLSR, kAddrDPIndexX);        // 0x56

const int kOpMVN_BlockMove          = OPMODE(kOpMVN, kAddrBlockMove);       // 0x54
const int kOpMVP_BlockMove          = OPMODE(kOpMVP, kAddrBlockMove);       // 0x44

const int kOpNOP_Implied            = OPMODE(kOpNOP, kAddrImplied);         // 0xea

const int kOpORA_Imm                = OPMODE(kOpORA, kAddrImm);             // 0x09
const int kOpORA_Abs                = OPMODE(kOpORA, kAddrAbs);             // 0x0d
const int kOpORA_AbsLong            = OPMODE(kOpORA, kAddrAbsLong);         // 0x0f
const int kOpORA_DP                 = OPMODE(kOpORA, kAddrDP);              // 0x05
const int kOpORA_DPInd              = OPMODE(kOpORA, kAddrDPInd);           // 0x12
const int kOpORA_DPIndLong          = OPMODE(kOpORA, kAddrDPIndLong);       // 0x07
const int kOpORA_AbsIndexX          = OPMODE(kOpORA, kAddrAbsIndexX);       // 0x1d
const int kOpORA_AbsIndexXLong      = OPMODE(kOpORA, kAddrAbsIndexXLong);   // 0x1f
const int kOpORA_AbsIndexY          = OPMODE(kOpORA, kAddrAbsIndexY);       // 0x19
const int kOpORA_DPIndexX           = OPMODE(kOpORA, kAddrDPIndexX);        // 0x15
const int kOpORA_DPIndexXInd        = OPMODE(kOpORA, kAddrDPIndexXInd);     // 0x01
const int kOpORA_DPIndIndexY        = OPMODE(kOpORA, kAddrDPIndIndexY);     // 0x11
const int kOpORA_DPIndIndexYLong    = OPMODE(kOpORA, kAddrDPIndIndexYLong); // 0x17
const int kOpORA_StackRel           = OPMODE(kOpORA, kAddrStackRel);        // 0x03
const int kOpORA_StackRelIndexY     = OPMODE(kOpORA, kAddrStackRelIndexY);  // 0x13

const int kOpPEA_StackAbs           = OPMODE(kOpPEA, kAddrStackAbs);        // 0xf4
const int kOpPEI_StackDPInd         = OPMODE(kOpPEI, kAddrStackDPInd);      // 0xd4
const int kOpPER_StackPCRel         = OPMODE(kOpPER, kAddrStackPCRel);      // 0x62
const int kOpPHA_StackPush          = OPMODE(kOpPHA, kAddrStackPush);       // 0x48
const int kOpPHB_StackPush          = OPMODE(kOpPHB, kAddrStackPush);       // 0x8b
const int kOpPHD_StackPush          = OPMODE(kOpPHD, kAddrStackPush);       // 0x0b
const int kOpPHK_StackPush          = OPMODE(kOpPHK, kAddrStackPush);       // 0x4b
const int kOpPHP_StackPush          = OPMODE(kOpPHP, kAddrStackPush);       // 0x08
const int kOpPHX_StackPush          = OPMODE(kOpPHX, kAddrStackPush);       // 0xda
const int kOpPHY_StackPush          = OPMODE(kOpPHY, kAddrStackPush);       // 0x5a

const int kOpPLA_StackPull          = OPMODE(kOpPLA, kAddrStackPull);       // 0x68
const int kOpPLB_StackPull          = OPMODE(kOpPLB, kAddrStackPull);       // 0xab
const int kOpPLD_StackPull          = OPMODE(kOpPLD, kAddrStackPull);       // 0x2b
const int kOpPLP_StackPull          = OPMODE(kOpPLP, kAddrStackPull);       // 0x28
const int kOpPLX_StackPull          = OPMODE(kOpPLX, kAddrStackPull);       // 0xfa
const int kOpPLY_StackPull          = OPMODE(kOpPLY, kAddrStackPull);       // 0x7a

const int kOpREP_Imm                = OPMODE(kOpREP, kAddrImm);             // 0xc2

const int kOpROL_Acc                = OPMODE(kOpROL, kAddrAcc);             // 0x2a
const int kOpROL_Abs                = OPMODE(kOpROL, kAddrAbs);             // 0x2e
const int kOpROL_DP                 = OPMODE(kOpROL, kAddrDP);              // 0x26
const int kOpROL_AbsIndexX          = OPMODE(kOpROL, kAddrAbsIndexX);       // 0x3e
const int kOpROL_DPIndexX           = OPMODE(kOpROL, kAddrDPIndexX);        // 0x36

const int kOpROR_Acc                = OPMODE(kOpROR, kAddrAcc);             // 0x6a
const int kOpROR_Abs                = OPMODE(kOpROR, kAddrAbs);             // 0x6e
const int kOpROR_DP                 = OPMODE(kOpROR, kAddrDP);              // 0x66
const int kOpROR_AbsIndexX          = OPMODE(kOpROR, kAddrAbsIndexX);       // 0x7e
const int kOpROR_DPIndexX           = OPMODE(kOpROR, kAddrDPIndexX);        // 0x76

const int kOpRTI_StackRTI           = OPMODE(kOpRTI, kAddrStackRTI);        // 0x40
const int kOpRTL_StackRTL           = OPMODE(kOpRTL, kAddrStackRTL);        // 0x6b
const int kOpRTS_StackRTS           = OPMODE(kOpRTS, kAddrStackRTS);        // 0x60

const int kOpSBC_Imm                = OPMODE(kOpSBC, kAddrImm);             // 0xe9
const int kOpSBC_Abs                = OPMODE(kOpSBC, kAddrAbs);             // 0xed
const int kOpSBC_AbsLong            = OPMODE(kOpSBC, kAddrAbsLong);         // 0xef
const int kOpSBC_DP                 = OPMODE(kOpSBC, kAddrDP);              // 0xe5
const int kOpSBC_DPInd              = OPMODE(kOpSBC, kAddrDPInd);           // 0xf2
const int kOpSBC_DPIndLong          = OPMODE(kOpSBC, kAddrDPIndLong);       // 0xe7
const int kOpSBC_AbsIndexX          = OPMODE(kOpSBC, kAddrAbsIndexX);       // 0xfd
const int kOpSBC_AbsIndexXLong      = OPMODE(kOpSBC, kAddrAbsIndexXLong);   // 0xff
const int kOpSBC_AbsIndexY          = OPMODE(kOpSBC, kAddrAbsIndexY);       // 0xf9
const int kOpSBC_DPIndexX           = OPMODE(kOpSBC, kAddrDPIndexX);        // 0xf5
const int kOpSBC_DPIndexXInd        = OPMODE(kOpSBC, kAddrDPIndexXInd);     // 0xe1
const int kOpSBC_DPIndIndexY        = OPMODE(kOpSBC, kAddrDPIndIndexY);     // 0xf1
const int kOpSBC_DPIndIndexYLong    = OPMODE(kOpSBC, kAddrDPIndIndexYLong); // 0xf7
const int kOpSBC_StackRel           = OPMODE(kOpSBC, kAddrStackRel);        // 0xe3
const int kOpSBC_StackRelIndexY     = OPMODE(kOpSBC, kAddrStackRelIndexY);  // 0xf3

const int kOpSEC_Implied            = OPMODE(kOpSEC, kAddrImplied);         // 0x38
const int kOpSED_Implied            = OPMODE(kOpSED, kAddrImplied);         // 0xf8
const int kOpSEI_Implied            = OPMODE(kOpSEI, kAddrImplied);         // 0x78

const int kOpSEP_Imm                = OPMODE(kOpSEP, kAddrImm);             // 0xe2

const int kOpSTA_Abs                = OPMODE(kOpSTA, kAddrAbs);             // 0x8d
const int kOpSTA_AbsLong            = OPMODE(kOpSTA, kAddrAbsLong);         // 0x8f
const int kOpSTA_DP                 = OPMODE(kOpSTA, kAddrDP);              // 0x85
const int kOpSTA_DPInd              = OPMODE(kOpSTA, kAddrDPInd);           // 0x92
const int kOpSTA_DPIndLong          = OPMODE(kOpSTA, kAddrDPIndLong);       // 0x87
const int kOpSTA_AbsIndexX          = OPMODE(kOpSTA, kAddrAbsIndexX);       // 0x9d
const int kOpSTA_AbsIndexXLong      = OPMODE(kOpSTA, kAddrAbsIndexXLong);   // 0x9f
const int kOpSTA_AbsIndexY          = OPMODE(kOpSTA, kAddrAbsIndexY);       // 0x99
const int kOpSTA_DPIndexX           = OPMODE(kOpSTA, kAddrDPIndexX);        // 0x95
const int kOpSTA_DPIndexXInd        = OPMODE(kOpSTA, kAddrDPIndexXInd);     // 0x81
const int kOpSTA_DPIndIndexY        = OPMODE(kOpSTA, kAddrDPIndIndexY);     // 0x91
const int kOpSTA_DPIndIndexYLong    = OPMODE(kOpSTA, kAddrDPIndIndexYLong); // 0x97
const int kOpSTA_StackRel           = OPMODE(kOpSTA, kAddrStackRel);        // 0x83
const int kOpSTA_StackRelIndexY     = OPMODE(kOpSTA, kAddrStackRelIndexY);  // 0x93

const int kOpSTP_Implied            = OPMODE(kOpSTP, kAddrImplied);         // 0xdb

const int kOpSTX_Abs                = OPMODE(kOpSTX, kAddrAbs);             // 0x8e
const int kOpSTX_DP                 = OPMODE(kOpSTX, kAddrDP);              // 0x86
const int kOpSTX_DPIndexY           = OPMODE(kOpSTX, kAddrDPIndexY);        // 0x96

const int kOpSTY_Abs                = OPMODE(kOpSTY, kAddrAbs);             // 0x8c
const int kOpSTY_DP                 = OPMODE(kOpSTY, kAddrDP);              // 0x84
const int kOpSTY_DPIndexX           = OPMODE(kOpSTY, kAddrDPIndexX);        // 0x94

const int kOpSTZ_Abs                = OPMODE(kOpSTZ, kAddrAbs);             // 0x9c
const int kOpSTZ_DP                 = OPMODE(kOpSTZ, kAddrDP);              // 0x64
const int kOpSTZ_AbsIndexX          = OPMODE(kOpSTZ, kAddrAbsIndexX);       // 0x9e
const int kOpSTZ_DPIndexX           = OPMODE(kOpSTZ, kAddrDPIndexX);        // 0x74

const int kOpTAX_Implied            = OPMODE(kOpTAX, kAddrImplied);         // 0xaa
const int kOpTAY_Implied            = OPMODE(kOpTAY, kAddrImplied);         // 0xa8
const int kOpTCD_Implied            = OPMODE(kOpTCD, kAddrImplied);         // 0x5b
const int kOpTCS_Implied            = OPMODE(kOpTCS, kAddrImplied);         // 0x1b
const int kOpTDC_Implied            = OPMODE(kOpTDC, kAddrImplied);         // 0x7b

const int kOpTRB_Abs                = OPMODE(kOpTRB, kAddrAbs);             // 0x1c
const int kOpTRB_DP                 = OPMODE(kOpTRB, kAddrDP);              // 0x14

const int kOpTSB_Abs                = OPMODE(kOpTSB, kAddrAbs);             // 0x0c
const int kOpTSB_DP                 = OPMODE(kOpTSB, kAddrDP);              // 0x04

const int kOpTSC_Implied            = OPMODE(kOpTSC, kAddrImplied);         // 0x3b
const int kOpTSX_Implied            = OPMODE(kOpTSX, kAddrImplied);         // 0xba
const int kOpTXA_Implied            = OPMODE(kOpTXA, kAddrImplied);         // 0x8a
const int kOpTXS_Implied            = OPMODE(kOpTXS, kAddrImplied);         // 0x9a
const int kOpTXY_Implied            = OPMODE(kOpTXY, kAddrImplied);         // 0x9b
const int kOpTYA_Implied            = OPMODE(kOpTYA, kAddrImplied);         // 0x98
const int kOpTYX_Implied            = OPMODE(kOpTYX, kAddrImplied);         // 0xbb

const int kOpWAI_Implied            = OPMODE(kOpWAI, kAddrImplied);         // 0xcb
const int kOpWDM_Implied            = OPMODE(kOpWDM, kAddrWDM);             // 0x42

const int kOpXBA_Implied            = OPMODE(kOpXBA, kAddrImplied);         // 0xeb
const int kOpXCE_Implied            = OPMODE(kOpXCE, kAddrImplied);         // 0xfb


/*
 * Map opcode bytes to instructions on all four CPUs (6502 65C02 65802 65816).
 *
 * Each entry has an OpCode enum in the low byte and an AddrMode enum in the
 * high byte.
 *
 * Instruction availability on 65802 and 65816 is generally identical.  We
 * give it its own column so that, if we need to model some 65802-specific
 * quirks, we can do it by assigning a different instruction.
 */
/*static*/ const ReformatDisasm65xxx::OpMap ReformatDisasm65xxx::kOpMap[] = {
    /*00*/{ kOpBRK_StackInt,        kOpBRK_StackInt,        kOpBRK_StackInt,        kOpBRK_StackInt },
    /*01*/{ kOpORA_DPIndexXInd,     kOpORA_DPIndexXInd,     kOpORA_DPIndexXInd,     kOpORA_DPIndexXInd },
    /*02*/{ kOpUnknown,             kOpUnknown,             kOpCOP_StackInt,        kOpCOP_StackInt },
    /*03*/{ kOpUnknown,             kOpUnknown,             kOpORA_StackRel,        kOpORA_StackRel },
    /*04*/{ kOpUnknown,             kOpTSB_DP,              kOpTSB_DP,              kOpTSB_DP },
    /*05*/{ kOpORA_DP,              kOpORA_DP,              kOpORA_DP,              kOpORA_DP },
    /*06*/{ kOpASL_DP,              kOpASL_DP,              kOpASL_DP,              kOpASL_DP },
    /*07*/{ kOpUnknown,             kOpUnknown,             kOpORA_DPIndLong,       kOpORA_DPIndLong },
    /*08*/{ kOpPHP_StackPush,       kOpPHP_StackPush,       kOpPHP_StackPush,       kOpPHP_StackPush },
    /*09*/{ kOpORA_Imm,             kOpORA_Imm,             kOpORA_Imm,             kOpORA_Imm },
    /*0a*/{ kOpASL_Acc,             kOpASL_Acc,             kOpASL_Acc,             kOpASL_Acc },
    /*0b*/{ kOpUnknown,             kOpUnknown,             kOpPHD_StackPush,       kOpPHD_StackPush },
    /*0c*/{ kOpUnknown,             kOpTSB_Abs,             kOpTSB_Abs,             kOpTSB_Abs },
    /*0d*/{ kOpORA_Abs,             kOpORA_Abs,             kOpORA_Abs,             kOpORA_Abs },
    /*0e*/{ kOpASL_Abs,             kOpASL_Abs,             kOpASL_Abs,             kOpASL_Abs },
    /*0f*/{ kOpUnknown,             kOpUnknown,             kOpORA_AbsLong,         kOpORA_AbsLong },
    /*10*/{ kOpBPL_PCRel,           kOpBPL_PCRel,           kOpBPL_PCRel,           kOpBPL_PCRel },
    /*11*/{ kOpORA_DPIndIndexY,     kOpORA_DPIndIndexY,     kOpORA_DPIndIndexY,     kOpORA_DPIndIndexY },
    /*12*/{ kOpUnknown,             kOpORA_DPInd,           kOpORA_DPInd,           kOpORA_DPInd },
    /*13*/{ kOpUnknown,             kOpUnknown,             kOpORA_StackRelIndexY,  kOpORA_StackRelIndexY },
    /*14*/{ kOpUnknown,             kOpTRB_DP,              kOpTRB_DP,              kOpTRB_DP },
    /*15*/{ kOpORA_DPIndexX,        kOpORA_DPIndexX,        kOpORA_DPIndexX,        kOpORA_DPIndexX },
    /*16*/{ kOpASL_DPIndexX,        kOpASL_DPIndexX,        kOpASL_DPIndexX,        kOpASL_DPIndexX },
    /*17*/{ kOpUnknown,             kOpUnknown,             kOpORA_DPIndIndexYLong, kOpORA_DPIndIndexYLong },
    /*18*/{ kOpCLC_Implied,         kOpCLC_Implied,         kOpCLC_Implied,         kOpCLC_Implied },
    /*19*/{ kOpORA_AbsIndexY,       kOpORA_AbsIndexY,       kOpORA_AbsIndexY,       kOpORA_AbsIndexY },
    /*1a*/{ kOpUnknown,             kOpINC_Acc,             kOpINC_Acc,             kOpINC_Acc },
    /*1b*/{ kOpUnknown,             kOpUnknown,             kOpTCS_Implied,         kOpTCS_Implied },
    /*1c*/{ kOpUnknown,             kOpTRB_Abs,             kOpTRB_Abs,             kOpTRB_Abs },
    /*1d*/{ kOpORA_AbsIndexX,       kOpORA_AbsIndexX,       kOpORA_AbsIndexX,       kOpORA_AbsIndexX },
    /*1e*/{ kOpASL_AbsIndexX,       kOpASL_AbsIndexX,       kOpASL_AbsIndexX,       kOpASL_AbsIndexX },
    /*1f*/{ kOpUnknown,             kOpUnknown,             kOpORA_AbsIndexXLong,   kOpORA_AbsIndexXLong },
    /*20*/{ kOpJSR_Abs,             kOpJSR_Abs,             kOpJSR_Abs,             kOpJSR_Abs },
    /*21*/{ kOpAND_DPIndexXInd,     kOpAND_DPIndexXInd,     kOpAND_DPIndexXInd,     kOpAND_DPIndexXInd },
    /*22*/{ kOpUnknown,             kOpUnknown,             kOpJSL_AbsLong,         kOpJSL_AbsLong },
    /*23*/{ kOpUnknown,             kOpUnknown,             kOpAND_StackRel,        kOpAND_StackRel },
    /*24*/{ kOpBIT_DP,              kOpBIT_DP,              kOpBIT_DP,              kOpBIT_DP },
    /*25*/{ kOpAND_DP,              kOpAND_DP,              kOpAND_DP,              kOpAND_DP },
    /*26*/{ kOpROL_DP,              kOpROL_DP,              kOpROL_DP,              kOpROL_DP },
    /*27*/{ kOpUnknown,             kOpUnknown,             kOpAND_DPIndLong,       kOpAND_DPIndLong },
    /*28*/{ kOpPLP_StackPull,       kOpPLP_StackPull,       kOpPLP_StackPull,       kOpPLP_StackPull },
    /*29*/{ kOpAND_Imm,             kOpAND_Imm,             kOpAND_Imm,             kOpAND_Imm },
    /*2a*/{ kOpROL_Acc,             kOpROL_Acc,             kOpROL_Acc,             kOpROL_Acc },
    /*2b*/{ kOpUnknown,             kOpUnknown,             kOpPLD_StackPull,       kOpPLD_StackPull },
    /*2c*/{ kOpBIT_Abs,             kOpBIT_Abs,             kOpBIT_Abs,             kOpBIT_Abs },
    /*2d*/{ kOpAND_Abs,             kOpAND_Abs,             kOpAND_Abs,             kOpAND_Abs },
    /*2e*/{ kOpROL_Abs,             kOpROL_Abs,             kOpROL_Abs,             kOpROL_Abs },
    /*2f*/{ kOpUnknown,             kOpUnknown,             kOpAND_AbsLong,         kOpAND_AbsLong },
    /*30*/{ kOpBMI_PCRel,           kOpBMI_PCRel,           kOpBMI_PCRel,           kOpBMI_PCRel },
    /*31*/{ kOpAND_DPIndIndexY,     kOpAND_DPIndIndexY,     kOpAND_DPIndIndexY,     kOpAND_DPIndIndexY },
    /*32*/{ kOpUnknown,             kOpAND_DPInd,           kOpAND_DPInd,           kOpAND_DPInd },
    /*33*/{ kOpUnknown,             kOpUnknown,             kOpAND_StackRelIndexY,  kOpAND_StackRelIndexY },
    /*34*/{ kOpUnknown,             kOpBIT_DPIndexX,        kOpBIT_DPIndexX,        kOpBIT_DPIndexX },
    /*35*/{ kOpAND_DPIndexX,        kOpAND_DPIndexX,        kOpAND_DPIndexX,        kOpAND_DPIndexX },
    /*36*/{ kOpROL_DPIndexX,        kOpROL_DPIndexX,        kOpROL_DPIndexX,        kOpROL_DPIndexX },
    /*37*/{ kOpUnknown,             kOpUnknown,             kOpAND_DPIndIndexYLong, kOpAND_DPIndIndexYLong },
    /*38*/{ kOpSEC_Implied,         kOpSEC_Implied,         kOpSEC_Implied,         kOpSEC_Implied },
    /*39*/{ kOpAND_AbsIndexY,       kOpAND_AbsIndexY,       kOpAND_AbsIndexY,       kOpAND_AbsIndexY },
    /*3a*/{ kOpUnknown,             kOpDEC_Acc,             kOpDEC_Acc,             kOpDEC_Acc },
    /*3b*/{ kOpUnknown,             kOpUnknown,             kOpTSC_Implied,         kOpTSC_Implied },
    /*3c*/{ kOpUnknown,             kOpBIT_AbsIndexX,       kOpBIT_AbsIndexX,       kOpBIT_AbsIndexX },
    /*3d*/{ kOpAND_AbsIndexX,       kOpAND_AbsIndexX,       kOpAND_AbsIndexX,       kOpAND_AbsIndexX },
    /*3e*/{ kOpROL_AbsIndexX,       kOpROL_AbsIndexX,       kOpROL_AbsIndexX,       kOpROL_AbsIndexX },
    /*3f*/{ kOpUnknown,             kOpUnknown,             kOpAND_AbsIndexXLong,   kOpAND_AbsIndexXLong },
    /*40*/{ kOpRTI_StackRTI,        kOpRTI_StackRTI,        kOpRTI_StackRTI,        kOpRTI_StackRTI },
    /*41*/{ kOpEOR_DPIndexXInd,     kOpEOR_DPIndexXInd,     kOpEOR_DPIndexXInd,     kOpEOR_DPIndexXInd },
    /*42*/{ kOpUnknown,             kOpUnknown,             kOpWDM_Implied,         kOpWDM_Implied },
    /*43*/{ kOpUnknown,             kOpUnknown,             kOpEOR_StackRel,        kOpEOR_StackRel },
    /*44*/{ kOpUnknown,             kOpUnknown,             kOpMVP_BlockMove,       kOpMVP_BlockMove },
    /*45*/{ kOpEOR_DP,              kOpEOR_DP,              kOpEOR_DP,              kOpEOR_DP },
    /*46*/{ kOpLSR_DP,              kOpLSR_DP,              kOpLSR_DP,              kOpLSR_DP },
    /*47*/{ kOpUnknown,             kOpUnknown,             kOpEOR_DPIndLong,       kOpEOR_DPIndLong },
    /*48*/{ kOpPHA_StackPush,       kOpPHA_StackPush,       kOpPHA_StackPush,       kOpPHA_StackPush },
    /*49*/{ kOpEOR_Imm,             kOpEOR_Imm,             kOpEOR_Imm,             kOpEOR_Imm },
    /*4a*/{ kOpLSR_Acc,             kOpLSR_Acc,             kOpLSR_Acc,             kOpLSR_Acc },
    /*4b*/{ kOpUnknown,             kOpUnknown,             kOpPHK_StackPush,       kOpPHK_StackPush },
    /*4c*/{ kOpJMP_Abs,             kOpJMP_Abs,             kOpJMP_Abs,             kOpJMP_Abs },
    /*4d*/{ kOpEOR_Abs,             kOpEOR_Abs,             kOpEOR_Abs,             kOpEOR_Abs },
    /*4e*/{ kOpLSR_Abs,             kOpLSR_Abs,             kOpLSR_Abs,             kOpLSR_Abs },
    /*4f*/{ kOpUnknown,             kOpUnknown,             kOpEOR_AbsLong,         kOpEOR_AbsLong },
    /*50*/{ kOpBVC_PCRel,           kOpBVC_PCRel,           kOpBVC_PCRel,           kOpBVC_PCRel },
    /*51*/{ kOpEOR_DPIndIndexY,     kOpEOR_DPIndIndexY,     kOpEOR_DPIndIndexY,     kOpEOR_DPIndIndexY },
    /*52*/{ kOpUnknown,             kOpEOR_DPInd,           kOpEOR_DPInd,           kOpEOR_DPInd },
    /*53*/{ kOpUnknown,             kOpUnknown,             kOpEOR_StackRelIndexY,  kOpEOR_StackRelIndexY },
    /*54*/{ kOpUnknown,             kOpUnknown,             kOpMVN_BlockMove,       kOpMVN_BlockMove },
    /*55*/{ kOpEOR_DPIndexX,        kOpEOR_DPIndexX,        kOpEOR_DPIndexX,        kOpEOR_DPIndexX },
    /*56*/{ kOpLSR_DPIndexX,        kOpLSR_DPIndexX,        kOpLSR_DPIndexX,        kOpLSR_DPIndexX },
    /*57*/{ kOpUnknown,             kOpUnknown,             kOpEOR_DPIndIndexYLong, kOpEOR_DPIndIndexYLong },
    /*58*/{ kOpCLI_Implied,         kOpCLI_Implied,         kOpCLI_Implied,         kOpCLI_Implied },
    /*59*/{ kOpEOR_AbsIndexY,       kOpEOR_AbsIndexY,       kOpEOR_AbsIndexY,       kOpEOR_AbsIndexY },
    /*5a*/{ kOpUnknown,             kOpPHY_StackPush,       kOpPHY_StackPush,       kOpPHY_StackPush },
    /*5b*/{ kOpUnknown,             kOpUnknown,             kOpTCD_Implied,         kOpTCD_Implied },
    /*5c*/{ kOpUnknown,             kOpUnknown,             kOpJMP_AbsLong,         kOpJMP_AbsLong },
    /*5d*/{ kOpEOR_AbsIndexX,       kOpEOR_AbsIndexX,       kOpEOR_AbsIndexX,       kOpEOR_AbsIndexX },
    /*5e*/{ kOpLSR_AbsIndexX,       kOpLSR_AbsIndexX,       kOpLSR_AbsIndexX,       kOpLSR_AbsIndexX },
    /*5f*/{ kOpUnknown,             kOpUnknown,             kOpEOR_AbsIndexXLong,   kOpEOR_AbsIndexXLong },
    /*60*/{ kOpRTS_StackRTS,        kOpRTS_StackRTS,        kOpRTS_StackRTS,        kOpRTS_StackRTS },
    /*61*/{ kOpADC_DPIndexXInd,     kOpADC_DPIndexXInd,     kOpADC_DPIndexXInd,     kOpADC_DPIndexXInd },
    /*62*/{ kOpUnknown,             kOpUnknown,             kOpPER_StackPCRel,      kOpPER_StackPCRel },
    /*63*/{ kOpUnknown,             kOpUnknown,             kOpADC_StackRel,        kOpADC_StackRel },
    /*64*/{ kOpUnknown,             kOpSTZ_DP,              kOpSTZ_DP,              kOpSTZ_DP },
    /*65*/{ kOpADC_DP,              kOpADC_DP,              kOpADC_DP,              kOpADC_DP },
    /*66*/{ kOpROR_DP,              kOpROR_DP,              kOpROR_DP,              kOpROR_DP },
    /*67*/{ kOpUnknown,             kOpUnknown,             kOpADC_DPIndLong,       kOpADC_DPIndLong },
    /*68*/{ kOpPLA_StackPull,       kOpPLA_StackPull,       kOpPLA_StackPull,       kOpPLA_StackPull },
    /*69*/{ kOpADC_Imm,             kOpADC_Imm,             kOpADC_Imm,             kOpADC_Imm },
    /*6a*/{ kOpROR_Acc,             kOpROR_Acc,             kOpROR_Acc,             kOpROR_Acc },
    /*6b*/{ kOpUnknown,             kOpUnknown,             kOpRTL_StackRTL,        kOpRTL_StackRTL },
    /*6c*/{ kOpJMP_AbsInd,          kOpJMP_AbsInd,          kOpJMP_AbsInd,          kOpJMP_AbsInd },
    /*6d*/{ kOpADC_Abs,             kOpADC_Abs,             kOpADC_Abs,             kOpADC_Abs },
    /*6e*/{ kOpROR_Abs,             kOpROR_Abs,             kOpROR_Abs,             kOpROR_Abs },
    /*6f*/{ kOpUnknown,             kOpUnknown,             kOpADC_AbsLong,         kOpADC_AbsLong },
    /*70*/{ kOpBVS_PCRel,           kOpBVS_PCRel,           kOpBVS_PCRel,           kOpBVS_PCRel },
    /*71*/{ kOpADC_DPIndIndexY,     kOpADC_DPIndIndexY,     kOpADC_DPIndIndexY,     kOpADC_DPIndIndexY },
    /*72*/{ kOpUnknown,             kOpADC_DPInd,           kOpADC_DPInd,           kOpADC_DPInd },
    /*73*/{ kOpUnknown,             kOpUnknown,             kOpADC_StackRelIndexY,  kOpADC_StackRelIndexY },
    /*74*/{ kOpUnknown,             kOpSTZ_DPIndexX,        kOpSTZ_DPIndexX,        kOpSTZ_DPIndexX },
    /*75*/{ kOpADC_DPIndexX,        kOpADC_DPIndexX,        kOpADC_DPIndexX,        kOpADC_DPIndexX },
    /*76*/{ kOpROR_DPIndexX,        kOpROR_DPIndexX,        kOpROR_DPIndexX,        kOpROR_DPIndexX },
    /*77*/{ kOpUnknown,             kOpUnknown,             kOpADC_DPIndIndexYLong, kOpADC_DPIndIndexYLong },
    /*78*/{ kOpSEI_Implied,         kOpSEI_Implied,         kOpSEI_Implied,         kOpSEI_Implied },
    /*79*/{ kOpADC_AbsIndexY,       kOpADC_AbsIndexY,       kOpADC_AbsIndexY,       kOpADC_AbsIndexY },
    /*7a*/{ kOpUnknown,             kOpPLY_StackPull,       kOpPLY_StackPull,       kOpPLY_StackPull },
    /*7b*/{ kOpUnknown,             kOpUnknown,             kOpTDC_Implied,         kOpTDC_Implied },
    /*7c*/{ kOpUnknown,             kOpJMP_AbsIndexXInd,    kOpJMP_AbsIndexXInd,    kOpJMP_AbsIndexXInd },
    /*7d*/{ kOpADC_AbsIndexX,       kOpADC_AbsIndexX,       kOpADC_AbsIndexX,       kOpADC_AbsIndexX },
    /*7e*/{ kOpROR_AbsIndexX,       kOpROR_AbsIndexX,       kOpROR_AbsIndexX,       kOpROR_AbsIndexX },
    /*7f*/{ kOpUnknown,             kOpUnknown,             kOpADC_AbsIndexXLong,   kOpADC_AbsIndexXLong },
    /*80*/{ kOpUnknown,             kOpBRA_PCRel,           kOpBRA_PCRel,           kOpBRA_PCRel },
    /*81*/{ kOpSTA_DPIndexXInd,     kOpSTA_DPIndexXInd,     kOpSTA_DPIndexXInd,     kOpSTA_DPIndexXInd },
    /*82*/{ kOpUnknown,             kOpUnknown,             kOpBRL_PCRelLong,       kOpBRL_PCRelLong },
    /*83*/{ kOpUnknown,             kOpUnknown,             kOpSTA_StackRel,        kOpSTA_StackRel },
    /*84*/{ kOpSTY_DP,              kOpSTY_DP,              kOpSTY_DP,              kOpSTY_DP },
    /*85*/{ kOpSTA_DP,              kOpSTA_DP,              kOpSTA_DP,              kOpSTA_DP },
    /*86*/{ kOpSTX_DP,              kOpSTX_DP,              kOpSTX_DP,              kOpSTX_DP },
    /*87*/{ kOpUnknown,             kOpUnknown,             kOpSTA_DPIndLong,       kOpSTA_DPIndLong },
    /*88*/{ kOpDEY_Implied,         kOpDEY_Implied,         kOpDEY_Implied,         kOpDEY_Implied },
    /*89*/{ kOpUnknown,             kOpBIT_Imm,             kOpBIT_Imm,             kOpBIT_Imm },
    /*8a*/{ kOpTXA_Implied,         kOpTXA_Implied,         kOpTXA_Implied,         kOpTXA_Implied },
    /*8b*/{ kOpUnknown,             kOpUnknown,             kOpPHB_StackPush,       kOpPHB_StackPush },
    /*8c*/{ kOpSTY_Abs,             kOpSTY_Abs,             kOpSTY_Abs,             kOpSTY_Abs },
    /*8d*/{ kOpSTA_Abs,             kOpSTA_Abs,             kOpSTA_Abs,             kOpSTA_Abs },
    /*8e*/{ kOpSTX_Abs,             kOpSTX_Abs,             kOpSTX_Abs,             kOpSTX_Abs },
    /*8f*/{ kOpUnknown,             kOpUnknown,             kOpSTA_AbsLong,         kOpSTA_AbsLong },
    /*90*/{ kOpBCC_PCRel,           kOpBCC_PCRel,           kOpBCC_PCRel,           kOpBCC_PCRel },
    /*91*/{ kOpSTA_DPIndIndexY,     kOpSTA_DPIndIndexY,     kOpSTA_DPIndIndexY,     kOpSTA_DPIndIndexY },
    /*92*/{ kOpUnknown,             kOpSTA_DPInd,           kOpSTA_DPInd,           kOpSTA_DPInd },
    /*93*/{ kOpUnknown,             kOpUnknown,             kOpSTA_StackRelIndexY,  kOpSTA_StackRelIndexY },
    /*94*/{ kOpSTY_DPIndexX,        kOpSTY_DPIndexX,        kOpSTY_DPIndexX,        kOpSTY_DPIndexX },
    /*95*/{ kOpSTA_DPIndexX,        kOpSTA_DPIndexX,        kOpSTA_DPIndexX,        kOpSTA_DPIndexX },
    /*96*/{ kOpSTX_DPIndexY,        kOpSTX_DPIndexY,        kOpSTX_DPIndexY,        kOpSTX_DPIndexY },
    /*97*/{ kOpUnknown,             kOpUnknown,             kOpSTA_DPIndIndexYLong, kOpSTA_DPIndIndexYLong },
    /*98*/{ kOpTYA_Implied,         kOpTYA_Implied,         kOpTYA_Implied,         kOpTYA_Implied },
    /*99*/{ kOpSTA_AbsIndexY,       kOpSTA_AbsIndexY,       kOpSTA_AbsIndexY,       kOpSTA_AbsIndexY },
    /*9a*/{ kOpTXS_Implied,         kOpTXS_Implied,         kOpTXS_Implied,         kOpTXS_Implied },
    /*9b*/{ kOpUnknown,             kOpUnknown,             kOpTXY_Implied,         kOpTXY_Implied },
    /*9c*/{ kOpUnknown,             kOpSTZ_Abs,             kOpSTZ_Abs,             kOpSTZ_Abs },
    /*9d*/{ kOpSTA_AbsIndexX,       kOpSTA_AbsIndexX,       kOpSTA_AbsIndexX,       kOpSTA_AbsIndexX },
    /*9e*/{ kOpUnknown,             kOpSTZ_AbsIndexX,       kOpSTZ_AbsIndexX,       kOpSTZ_AbsIndexX },
    /*9f*/{ kOpUnknown,             kOpUnknown,             kOpSTA_AbsIndexXLong,   kOpSTA_AbsIndexXLong },
    /*a0*/{ kOpLDY_Imm,             kOpLDY_Imm,             kOpLDY_Imm,             kOpLDY_Imm },
    /*a1*/{ kOpLDA_DPIndexXInd,     kOpLDA_DPIndexXInd,     kOpLDA_DPIndexXInd,     kOpLDA_DPIndexXInd },
    /*a2*/{ kOpLDX_Imm,             kOpLDX_Imm,             kOpLDX_Imm,             kOpLDX_Imm },
    /*a3*/{ kOpUnknown,             kOpUnknown,             kOpLDA_StackRel,        kOpLDA_StackRel },
    /*a4*/{ kOpLDY_DP,              kOpLDY_DP,              kOpLDY_DP,              kOpLDY_DP },
    /*a5*/{ kOpLDA_DP,              kOpLDA_DP,              kOpLDA_DP,              kOpLDA_DP },
    /*a6*/{ kOpLDX_DP,              kOpLDX_DP,              kOpLDX_DP,              kOpLDX_DP },
    /*a7*/{ kOpUnknown,             kOpUnknown,             kOpLDA_DPIndLong,       kOpLDA_DPIndLong },
    /*a8*/{ kOpTAY_Implied,         kOpTAY_Implied,         kOpTAY_Implied,         kOpTAY_Implied },
    /*a9*/{ kOpLDA_Imm,             kOpLDA_Imm,             kOpLDA_Imm,             kOpLDA_Imm },
    /*aa*/{ kOpTAX_Implied,         kOpTAX_Implied,         kOpTAX_Implied,         kOpTAX_Implied },
    /*ab*/{ kOpUnknown,             kOpUnknown,             kOpPLB_StackPull,       kOpPLB_StackPull },
    /*ac*/{ kOpLDY_Abs,             kOpLDY_Abs,             kOpLDY_Abs,             kOpLDY_Abs },
    /*ad*/{ kOpLDA_Abs,             kOpLDA_Abs,             kOpLDA_Abs,             kOpLDA_Abs },
    /*ae*/{ kOpLDX_Abs,             kOpLDX_Abs,             kOpLDX_Abs,             kOpLDX_Abs },
    /*af*/{ kOpUnknown,             kOpUnknown,             kOpLDA_AbsLong,         kOpLDA_AbsLong },
    /*b0*/{ kOpBCS_PCRel,           kOpBCS_PCRel,           kOpBCS_PCRel,           kOpBCS_PCRel },
    /*b1*/{ kOpLDA_DPIndIndexY,     kOpLDA_DPIndIndexY,     kOpLDA_DPIndIndexY,     kOpLDA_DPIndIndexY },
    /*b2*/{ kOpUnknown,             kOpLDA_DPInd,           kOpLDA_DPInd,           kOpLDA_DPInd },
    /*b3*/{ kOpUnknown,             kOpUnknown,             kOpLDA_StackRelIndexY,  kOpLDA_StackRelIndexY },
    /*b4*/{ kOpLDY_DPIndexX,        kOpLDY_DPIndexX,        kOpLDY_DPIndexX,        kOpLDY_DPIndexX },
    /*b5*/{ kOpLDA_DPIndexX,        kOpLDA_DPIndexX,        kOpLDA_DPIndexX,        kOpLDA_DPIndexX },
    /*b6*/{ kOpLDX_DPIndexY,        kOpLDX_DPIndexY,        kOpLDX_DPIndexY,        kOpLDX_DPIndexY },
    /*b7*/{ kOpUnknown,             kOpUnknown,             kOpLDA_DPIndIndexYLong, kOpLDA_DPIndIndexYLong },
    /*b8*/{ kOpCLV_Implied,         kOpCLV_Implied,         kOpCLV_Implied,         kOpCLV_Implied },
    /*b9*/{ kOpLDA_AbsIndexY,       kOpLDA_AbsIndexY,       kOpLDA_AbsIndexY,       kOpLDA_AbsIndexY },
    /*ba*/{ kOpTSX_Implied,         kOpTSX_Implied,         kOpTSX_Implied,         kOpTSX_Implied },
    /*bb*/{ kOpUnknown,             kOpUnknown,             kOpTYX_Implied,         kOpTYX_Implied },
    /*bc*/{ kOpLDY_AbsIndexX,       kOpLDY_AbsIndexX,       kOpLDY_AbsIndexX,       kOpLDY_AbsIndexX },
    /*bd*/{ kOpLDA_AbsIndexX,       kOpLDA_AbsIndexX,       kOpLDA_AbsIndexX,       kOpLDA_AbsIndexX },
    /*be*/{ kOpLDX_AbsIndexY,       kOpLDX_AbsIndexY,       kOpLDX_AbsIndexY,       kOpLDX_AbsIndexY },
    /*bf*/{ kOpUnknown,             kOpUnknown,             kOpLDA_AbsIndexXLong,   kOpLDA_AbsIndexXLong },
    /*c0*/{ kOpCPY_Imm,             kOpCPY_Imm,             kOpCPY_Imm,             kOpCPY_Imm },
    /*c1*/{ kOpCMP_DPIndexXInd,     kOpCMP_DPIndexXInd,     kOpCMP_DPIndexXInd,     kOpCMP_DPIndexXInd },
    /*c2*/{ kOpUnknown,             kOpUnknown,             kOpREP_Imm,             kOpREP_Imm },
    /*c3*/{ kOpUnknown,             kOpUnknown,             kOpCMP_StackRel,        kOpCMP_StackRel },
    /*c4*/{ kOpCPY_DP,              kOpCPY_DP,              kOpCPY_DP,              kOpCPY_DP },
    /*c5*/{ kOpCMP_DP,              kOpCMP_DP,              kOpCMP_DP,              kOpCMP_DP },
    /*c6*/{ kOpDEC_DP,              kOpDEC_DP,              kOpDEC_DP,              kOpDEC_DP },
    /*c7*/{ kOpUnknown,             kOpUnknown,             kOpCMP_DPIndLong,       kOpCMP_DPIndLong },
    /*c8*/{ kOpINY_Implied,         kOpINY_Implied,         kOpINY_Implied,         kOpINY_Implied },
    /*c9*/{ kOpCMP_Imm,             kOpCMP_Imm,             kOpCMP_Imm,             kOpCMP_Imm },
    /*ca*/{ kOpDEX_Implied,         kOpDEX_Implied,         kOpDEX_Implied,         kOpDEX_Implied },
    /*cb*/{ kOpUnknown,             kOpUnknown,             kOpWAI_Implied,         kOpWAI_Implied },
    /*cc*/{ kOpCPY_Abs,             kOpCPY_Abs,             kOpCPY_Abs,             kOpCPY_Abs },
    /*cd*/{ kOpCMP_Abs,             kOpCMP_Abs,             kOpCMP_Abs,             kOpCMP_Abs },
    /*ce*/{ kOpDEC_Abs,             kOpDEC_Abs,             kOpDEC_Abs,             kOpDEC_Abs },
    /*cf*/{ kOpUnknown,             kOpUnknown,             kOpCMP_AbsLong,         kOpCMP_AbsLong },
    /*d0*/{ kOpBNE_PCRel,           kOpBNE_PCRel,           kOpBNE_PCRel,           kOpBNE_PCRel },
    /*d1*/{ kOpCMP_DPIndIndexY,     kOpCMP_DPIndIndexY,     kOpCMP_DPIndIndexY,     kOpCMP_DPIndIndexY },
    /*d2*/{ kOpUnknown,             kOpCMP_DPInd,           kOpCMP_DPInd,           kOpCMP_DPInd },
    /*d3*/{ kOpUnknown,             kOpUnknown,             kOpCMP_StackRelIndexY,  kOpCMP_StackRelIndexY },
    /*d4*/{ kOpUnknown,             kOpUnknown,             kOpPEI_StackDPInd,      kOpPEI_StackDPInd },
    /*d5*/{ kOpCMP_DPIndexX,        kOpCMP_DPIndexX,        kOpCMP_DPIndexX,        kOpCMP_DPIndexX },
    /*d6*/{ kOpDEC_DPIndexX,        kOpDEC_DPIndexX,        kOpDEC_DPIndexX,        kOpDEC_DPIndexX },
    /*d7*/{ kOpUnknown,             kOpUnknown,             kOpCMP_DPIndIndexYLong, kOpCMP_DPIndIndexYLong },
    /*d8*/{ kOpCLD_Implied,         kOpCLD_Implied,         kOpCLD_Implied,         kOpCLD_Implied },
    /*d9*/{ kOpCMP_AbsIndexY,       kOpCMP_AbsIndexY,       kOpCMP_AbsIndexY,       kOpCMP_AbsIndexY },
    /*da*/{ kOpUnknown,             kOpPHX_StackPush,       kOpPHX_StackPush,       kOpPHX_StackPush },
    /*db*/{ kOpUnknown,             kOpUnknown,             kOpSTP_Implied,         kOpSTP_Implied },
    /*dc*/{ kOpUnknown,             kOpUnknown,             kOpJML_AbsIndLong,      kOpJML_AbsIndLong },
    /*dd*/{ kOpCMP_AbsIndexX,       kOpCMP_AbsIndexX,       kOpCMP_AbsIndexX,       kOpCMP_AbsIndexX },
    /*de*/{ kOpDEC_AbsIndexX,       kOpDEC_AbsIndexX,       kOpDEC_AbsIndexX,       kOpDEC_AbsIndexX },
    /*df*/{ kOpUnknown,             kOpUnknown,             kOpCMP_AbsIndexXLong,   kOpCMP_AbsIndexXLong },
    /*e0*/{ kOpCPX_Imm,             kOpCPX_Imm,             kOpCPX_Imm,             kOpCPX_Imm },
    /*e1*/{ kOpSBC_DPIndexXInd,     kOpSBC_DPIndexXInd,     kOpSBC_DPIndexXInd,     kOpSBC_DPIndexXInd },
    /*e2*/{ kOpUnknown,             kOpUnknown,             kOpSEP_Imm,             kOpSEP_Imm },
    /*e3*/{ kOpUnknown,             kOpUnknown,             kOpSBC_StackRel,        kOpSBC_StackRel },
    /*e4*/{ kOpCPX_DP,              kOpCPX_DP,              kOpCPX_DP,              kOpCPX_DP },
    /*e5*/{ kOpSBC_DP,              kOpSBC_DP,              kOpSBC_DP,              kOpSBC_DP },
    /*e6*/{ kOpINC_DP,              kOpINC_DP,              kOpINC_DP,              kOpINC_DP },
    /*e7*/{ kOpUnknown,             kOpUnknown,             kOpSBC_DPIndLong,       kOpSBC_DPIndLong },
    /*e8*/{ kOpINX_Implied,         kOpINX_Implied,         kOpINX_Implied,         kOpINX_Implied },
    /*e9*/{ kOpSBC_Imm,             kOpSBC_Imm,             kOpSBC_Imm,             kOpSBC_Imm },
    /*ea*/{ kOpNOP_Implied,         kOpNOP_Implied,         kOpNOP_Implied,         kOpNOP_Implied },
    /*eb*/{ kOpUnknown,             kOpUnknown,             kOpXBA_Implied,         kOpXBA_Implied },
    /*ec*/{ kOpCPX_Abs,             kOpCPX_Abs,             kOpCPX_Abs,             kOpCPX_Abs },
    /*ed*/{ kOpSBC_Abs,             kOpSBC_Abs,             kOpSBC_Abs,             kOpSBC_Abs },
    /*ee*/{ kOpINC_Abs,             kOpINC_Abs,             kOpINC_Abs,             kOpINC_Abs },
    /*ef*/{ kOpUnknown,             kOpUnknown,             kOpSBC_AbsLong,         kOpSBC_AbsLong },
    /*f0*/{ kOpBEQ_PCRel,           kOpBEQ_PCRel,           kOpBEQ_PCRel,           kOpBEQ_PCRel },
    /*f1*/{ kOpSBC_DPIndIndexY,     kOpSBC_DPIndIndexY,     kOpSBC_DPIndIndexY,     kOpSBC_DPIndIndexY },
    /*f2*/{ kOpUnknown,             kOpSBC_DPInd,           kOpSBC_DPInd,           kOpSBC_DPInd },
    /*f3*/{ kOpUnknown,             kOpUnknown,             kOpSBC_StackRelIndexY,  kOpSBC_StackRelIndexY },
    /*f4*/{ kOpUnknown,             kOpUnknown,             kOpPEA_StackAbs,        kOpPEA_StackAbs },
    /*f5*/{ kOpSBC_DPIndexX,        kOpSBC_DPIndexX,        kOpSBC_DPIndexX,        kOpSBC_DPIndexX },
    /*f6*/{ kOpINC_DPIndexX,        kOpINC_DPIndexX,        kOpINC_DPIndexX,        kOpINC_DPIndexX },
    /*f7*/{ kOpUnknown,             kOpUnknown,             kOpSBC_DPIndIndexYLong, kOpSBC_DPIndIndexYLong },
    /*f8*/{ kOpSED_Implied,         kOpSED_Implied,         kOpSED_Implied,         kOpSED_Implied },
    /*f9*/{ kOpSBC_AbsIndexY,       kOpSBC_AbsIndexY,       kOpSBC_AbsIndexY,       kOpSBC_AbsIndexY },
    /*fa*/{ kOpUnknown,             kOpPLX_StackPull,       kOpPLX_StackPull,       kOpPLX_StackPull },
    /*fb*/{ kOpUnknown,             kOpUnknown,             kOpXCE_Implied,         kOpXCE_Implied },
    /*fc*/{ kOpUnknown,             kOpUnknown,             kOpJSR_AbsIndexXInd,    kOpJSR_AbsIndexXInd },
    /*fd*/{ kOpSBC_AbsIndexX,       kOpSBC_AbsIndexX,       kOpSBC_AbsIndexX,       kOpSBC_AbsIndexX },
    /*fe*/{ kOpINC_AbsIndexX,       kOpINC_AbsIndexX,       kOpINC_AbsIndexX,       kOpINC_AbsIndexX },
    /*ff*/{ kOpUnknown,             kOpUnknown,             kOpSBC_AbsIndexXLong,   kOpSBC_AbsIndexXLong },
};

/*
 * Validate the contents of kOpMap.
 *
 * Returns "true" on success, "false" on failure.
 */
bool ReformatDisasm65xxx::ValidateOpMap(void)
{
    if (NELEM(kOpMap) != 256) {
        ASSERT(false);
        return false;
    }

#ifdef _DEBUG
    /*
     * Verify that all entries on one line have the same opcode and address
     * mode.  For "hidden" 6502 opcodes this won't hold, so we'll have to
     * handle those separately.
     *
     * This assumes that later processors always include everything that the
     * previous CPUs did.  This turns out to be the case.
     */
    int i, j;
    for (i = 0; i < NELEM(kOpMap); i++) {
        if ((kOpMap[i].opAndAddr[kCPU6502] != kOpUnknown &&
             kOpMap[i].opAndAddr[kCPU6502] != kOpMap[i].opAndAddr[kCPU65C02]) ||
            (kOpMap[i].opAndAddr[kCPU65C02] != kOpUnknown &&
             kOpMap[i].opAndAddr[kCPU65C02] != kOpMap[i].opAndAddr[kCPU65802]) ||
            (kOpMap[i].opAndAddr[kCPU65802] != kOpUnknown &&
             kOpMap[i].opAndAddr[kCPU65802] != kOpMap[i].opAndAddr[kCPU65816]))
        {
            LOGI("OpMap GLITCH: inconsistent values for entry 0x%02x", i);
            assert(false);
            return false;
        }
    }

    /* O(n^2) duplicate opcode check */
    for (i = 0; i < NELEM(kOpMap)-1; i++) {
        if (kOpMap[i].opAndAddr[kCPU65816] == kOpUnknown)
            continue;
        for (j = i+1; j < NELEM(kOpMap); j++) {
            if (kOpMap[j].opAndAddr[kCPU65816] == kOpUnknown)
                continue;
            if (kOpMap[i].opAndAddr[kCPU65816] == kOpMap[j].opAndAddr[kCPU65816])
            {
                LOGI("OpMap GLITCH: entries 0x%02x and 0x%02x match", i, j);
                assert(false);
                return false;
            }
        }
    }
#endif /*_DEBUG*/

    return true;
}

/*
 * OpCode details.
 */
/*static*/ const ReformatDisasm65xxx::OpCodeDetails ReformatDisasm65xxx::kOpCodeDetails[] = {
    { kOpCodeUnknown, "???" },
    { kOpADC, "ADC" },
    { kOpAND, "AND" },
    { kOpASL, "ASL" },
    { kOpBCC, "BCC" },
    { kOpBCS, "BCS" },
    { kOpBEQ, "BEQ" },
    { kOpBIT, "BIT" },
    { kOpBMI, "BMI" },
    { kOpBNE, "BNE" },
    { kOpBPL, "BPL" },
    { kOpBRA, "BRA" },
    { kOpBRK, "BRK" },
    { kOpBRL, "BRL" },
    { kOpBVC, "BVC" },
    { kOpBVS, "BVS" },
    { kOpCLC, "CLC" },
    { kOpCLD, "CLD" },
    { kOpCLI, "CLI" },
    { kOpCLV, "CLV" },
    { kOpCMP, "CMP" },
    { kOpCOP, "COP" },
    { kOpCPX, "CPX" },
    { kOpCPY, "CPY" },
    { kOpDEC, "DEC" },
    { kOpDEX, "DEX" },
    { kOpDEY, "DEY" },
    { kOpEOR, "EOR" },
    { kOpINC, "INC" },
    { kOpINX, "INX" },
    { kOpINY, "INY" },
    { kOpJML, "JML" },
    { kOpJMP, "JMP" },
    { kOpJSL, "JSL" },
    { kOpJSR, "JSR" },
    { kOpLDA, "LDA" },
    { kOpLDX, "LDX" },
    { kOpLDY, "LDY" },
    { kOpLSR, "LSR" },
    { kOpMVN, "MVN" },
    { kOpMVP, "MVP" },
    { kOpNOP, "NOP" },
    { kOpORA, "ORA" },
    { kOpPEA, "PEA" },
    { kOpPEI, "PEI" },
    { kOpPER, "PER" },
    { kOpPHA, "PHA" },
    { kOpPHB, "PHB" },
    { kOpPHD, "PHD" },
    { kOpPHK, "PHK" },
    { kOpPHP, "PHP" },
    { kOpPHX, "PHX" },
    { kOpPHY, "PHY" },
    { kOpPLA, "PLA" },
    { kOpPLB, "PLB" },
    { kOpPLD, "PLD" },
    { kOpPLP, "PLP" },
    { kOpPLX, "PLX" },
    { kOpPLY, "PLY" },
    { kOpREP, "REP" },
    { kOpROL, "ROL" },
    { kOpROR, "ROR" },
    { kOpRTI, "RTI" },
    { kOpRTL, "RTL" },
    { kOpRTS, "RTS" },
    { kOpSBC, "SBC" },
    { kOpSEC, "SEC" },
    { kOpSED, "SED" },
    { kOpSEI, "SEI" },
    { kOpSEP, "SEP" },
    { kOpSTA, "STA" },
    { kOpSTP, "STP" },
    { kOpSTX, "STX" },
    { kOpSTY, "STY" },
    { kOpSTZ, "STZ" },
    { kOpTAX, "TAX" },
    { kOpTAY, "TAY" },
    { kOpTCD, "TCD" },
    { kOpTCS, "TCS" },
    { kOpTDC, "TDC" },
    { kOpTRB, "TRB" },
    { kOpTSB, "TSB" },
    { kOpTSC, "TSC" },
    { kOpTSX, "TSX" },
    { kOpTXA, "TXA" },
    { kOpTXS, "TXS" },
    { kOpTXY, "TXY" },
    { kOpTYA, "TYA" },
    { kOpTYX, "TYX" },
    { kOpWAI, "WAI" },
    { kOpWDM, "WDM" },
    { kOpXBA, "XBA" },
    { kOpXCE, "XCE" },
};

/*
 * Validate the contents of kOpCodeDetails.
 *
 * Returns "true" on success, "false" on failure.
 */
bool ReformatDisasm65xxx::ValidateOpCodeDetails(void)
{
    if (NELEM(kOpCodeDetails) != kOpCodeMAX) {
        LOGI("Found %d entries in details, max=%d",
            NELEM(kOpCodeDetails), kOpCodeMAX);
        assert(false);
        return false;
    }

#ifdef _DEBUG
    /* O(n^2) duplicate string check */
    for (int i = 0; i < NELEM(kOpCodeDetails)-1; i++) {
        for (int j = i+1; j < NELEM(kOpCodeDetails); j++) {
            if (stricmp(kOpCodeDetails[i].mnemonic,
                           kOpCodeDetails[j].mnemonic) == 0)
            {
                LOGI("OpCodeDetails GLITCH: entries %d and %d match (%hs)",
                    i, j, kOpCodeDetails[i].mnemonic);
                assert(false);
                return false;
            }
        }
    }
#endif /*_DEBUG*/

    return true;
}
