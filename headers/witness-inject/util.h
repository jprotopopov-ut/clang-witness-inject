#ifndef WITNESS_INEJCT_UTIL_H_
#define WITNESS_INEJCT_UTIL_H_

#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/ASTContext.h>
#include <clang/Basic/SourceLocation.h>

namespace witness_inject::util {
    clang::DynTypedNode largestASTNodeStartingAt(clang::ASTContext &, clang::SourceLocation);
}

#endif
