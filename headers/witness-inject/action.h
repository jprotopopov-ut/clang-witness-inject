#ifndef WITNESS_INJECT_ACTION_H_
#define WITNESS_INJECT_ACTION_H_

#include "witness-inject/inject.h"
#include "witness-inject/witness.h"

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Rewrite/Core/Rewriter.h>

namespace witness_inject {

    class WitnessInjectAction : public clang::ASTFrontendAction {
     private:
        const WitnessInjectionConfig &config;
        clang::Rewriter rewriter;

     public:
        explicit WitnessInjectAction(const WitnessInjectionConfig &);
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &, clang::StringRef) final;
        void EndSourceFileAction(void) final;
    };

    class WitnessInjectActionFactory : public clang::tooling::FrontendActionFactory {
     private:
        const WitnessInjectionConfig &config;

     public:
        explicit WitnessInjectActionFactory(const WitnessInjectionConfig &);
        std::unique_ptr<clang::FrontendAction> create() final;
    };
}

#endif
