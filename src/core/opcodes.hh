//
// opcodes.hh
//
// 
//

#define DEFINE_OPCODES \
    DEFINE_OP(_INTERP) \
    DEFINE_OP(_TAILINTERP) \
    DEFINE_OP(_LITERAL) \
    DEFINE_OP(_INT) \
    DEFINE_OP(_RETURN) \
    DEFINE_OP(_BRANCH) \
    DEFINE_OP(_ZBRANCH) \
    DEFINE_OP(NOP) \
    DEFINE_OP(_RECURSE) \
    DEFINE_OP(DROP) \
    DEFINE_OP(DUP) \
    DEFINE_OP(OVER) \
    DEFINE_OP(ROT) \
    DEFINE_OP(_ROTn) \
    DEFINE_OP(SWAP) \
    DEFINE_OP(ZERO) \
    DEFINE_OP(ONE) \
    DEFINE_OP(EQ) \
    DEFINE_OP(NE) \
    DEFINE_OP(EQ_ZERO) \
    DEFINE_OP(NE_ZERO) \
    DEFINE_OP(GE) \
    DEFINE_OP(GT) \
    DEFINE_OP(GT_ZERO) \
    DEFINE_OP(LE) \
    DEFINE_OP(LT) \
    DEFINE_OP(LT_ZERO) \
    DEFINE_OP(ABS) \
    DEFINE_OP(MAX) \
    DEFINE_OP(MIN) \
    DEFINE_OP(DIV) \
    DEFINE_OP(MOD) \
    DEFINE_OP(MINUS) \
    DEFINE_OP(MULT) \
    DEFINE_OP(PLUS) \
    DEFINE_OP(CALL) \
    DEFINE_OP(NULL_) \
    DEFINE_OP(LENGTH) \
    DEFINE_OP(IFELSE) \
    DEFINE_OP(DEFINE) \
    DEFINE_OP(_GETARG) \
    DEFINE_OP(_SETARG) \
    DEFINE_OP(_LOCALS) \
    DEFINE_OP(_DROPARGS) \
    DEFINE_OP(PRINT) \
    DEFINE_OP(SP) \
    DEFINE_OP(NL) \
    DEFINE_OP(NLQ)

DEFINE_OPCODES

#undef DEFINE_OP
#undef DEFINE_OPCODES