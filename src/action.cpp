#include "witness-inject/action.h"
#include "witness-inject/inject.h"

namespace witness_inject {

    WitnessInjectAction::WitnessInjectAction(const WitnessInjectionConfig &config)
        : config{config} {}
        
    std::unique_ptr<clang::ASTConsumer> WitnessInjectAction::CreateASTConsumer(clang::CompilerInstance &ci, clang::StringRef inFile) {
        this->rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
        return std::make_unique<witness_inject::WitnessInjectASTConsumer>(this->config, this->rewriter);
    }

    void WitnessInjectAction::EndSourceFileAction(void) {
        this->rewriter.overwriteChangedFiles();
    }

    WitnessInjectActionFactory::WitnessInjectActionFactory(const WitnessInjectionConfig &config)
        : config{config} {}

    std::unique_ptr<clang::FrontendAction> WitnessInjectActionFactory::create() {
        return std::make_unique<WitnessInjectAction>(this->config);
    }
}
