#ifndef WITNESS_INJECT_INJECT_H_
#define WITNESS_INJECT_INJECT_H_

#include "witness-inject/witness.h"

#include <string>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTTypeTraits.h>
#include <clang/Rewrite/Core/Rewriter.h>

namespace witness_inject {

    struct WitnessInjectionConfig {
        const std::vector<witness_inject::witness::Entry> &witness;
        std::string assertFn{"assert"};
    };

    class WitnessInjectASTConsumer : public clang::ASTConsumer {
     private:
        const WitnessInjectionConfig &config;
        clang::Rewriter &rewriter;

        void InjectLocationInvariant(clang::ASTContext &, clang::FileID, const witness::Invariant &);
        void InjectLoopInvariant(clang::ASTContext &, clang::FileID, const witness::Invariant &);
        void InjectLoopInvariantAt(clang::ASTContext &, const clang::Stmt *, const witness::Invariant &);
        void InjectInvariantAt(clang::ASTContext &, clang::SourceLocation, clang::DynTypedNode, const witness::Invariant &);
        void InjectLocation(clang::ASTContext &, clang::SourceLocation, const witness::Location &);

     public:
        explicit WitnessInjectASTConsumer(const WitnessInjectionConfig &, clang::Rewriter &);
        void HandleTranslationUnit(clang::ASTContext &) final;
    };
}

#endif
