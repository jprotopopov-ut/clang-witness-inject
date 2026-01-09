#ifndef WITNESS_INJECT_INJECT_H_
#define WITNESS_INJECT_INJECT_H_

#include "witness-inject/witness.h"

#include <string>
#include <vector>

#include <clang/AST/ASTConsumer.h>
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

        void InjectInvariant(clang::ASTContext &, clang::FileID, const witness::Invariant &);

     public:
        explicit WitnessInjectASTConsumer(const WitnessInjectionConfig &, clang::Rewriter &);
        void HandleTranslationUnit(clang::ASTContext &) final;
    };
}

#endif
