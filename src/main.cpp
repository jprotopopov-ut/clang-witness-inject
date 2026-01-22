
#include "witness-inject/inject.h"
#include "witness-inject/witness.h"
#include "witness-inject/action.h"

#include <memory>
#include <string>
#include <vector>

#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/Format.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/CommandLine.h>

using namespace witness_inject;

static llvm::cl::OptionCategory ToolCat("witness-inject");
static llvm::cl::opt<std::string> WitnessYamlPath("witness-yaml", llvm::cl::desc("Witness YAML file"), llvm::cl::Required, llvm::cl::cat(ToolCat));
static llvm::cl::opt<std::string> AssertFn("assert-fn", llvm::cl::desc("Assert function"), llvm::cl::Optional, llvm::cl::cat(ToolCat), llvm::cl::init("assert"));
static llvm::cl::opt<bool> SkipSwitchCases("skip-switch-cases", llvm::cl::desc("Skip invariants attached to case and default labels in switch statement"), llvm::cl::Optional, llvm::cl::cat(ToolCat), llvm::cl::init(false));

static llvm::cl::list<std::string> ClangArgs(llvm::cl::Sink);

int main(int argc, const char **argv) {
    llvm::cl::ParseCommandLineOptions(argc, argv, ToolCat.getName());

    std::vector<std::string> sourceFiles;
    std::vector<std::string> compilerFlags;
    for (const auto &arg : ClangArgs) {
        if (!arg.empty() && arg[0] != '-') {
            sourceFiles.push_back(arg);
        } else {
            compilerFlags.push_back(arg);
        }
    }

    auto yamlBuf = llvm::MemoryBuffer::getFile(WitnessYamlPath);
    if (!yamlBuf) {
        llvm::errs() << "Failed to open " << WitnessYamlPath << "\n";
        return 1;
    }

    std::vector<witness::Entry> witness;
    witness::Entry::ReadInto(WitnessYamlPath, (*yamlBuf)->getMemBufferRef(), witness);

    clang::tooling::FixedCompilationDatabase Compilations(".", compilerFlags);
    clang::tooling::ClangTool Tool(Compilations, sourceFiles);
    
    return Tool.run(new WitnessInjectActionFactory(witness_inject::WitnessInjectionConfig {
        witness,
        AssertFn,
        SkipSwitchCases
    }));
}
