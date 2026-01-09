#ifndef WITNESS_INJECT_WITNESS_H_
#define WITNESS_INJECT_WITNESS_H_

#include <string>
#include <cstdint>
#include <vector>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBufferRef.h>

namespace witness_inject::witness {

    struct Location {
        std::string filepath;
        std::uint64_t line;
        std::uint32_t column;
        std::string function;
    };

    enum class InvariantType {
        Location,
        Loop
    };

    enum class InvariantFormat {
        CExpression
    };

    struct Invariant {
        InvariantType type;
        Location location;
        std::string value;
        InvariantFormat format;
    };

    struct InvariantItem {
        Invariant invariant;
    };

    enum class EntryType {
        InvariantSet
    };

    struct Entry {
        EntryType entry_type;
        std::vector<InvariantItem> content;

        static void ReadInto(llvm::StringRef, llvm::MemoryBufferRef, std::vector<Entry> &);
    };
};

#endif
