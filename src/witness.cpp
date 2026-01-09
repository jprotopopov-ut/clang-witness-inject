#include "witness-inject/witness.h"

#include <stdexcept>
#include <string>

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/YAMLTraits.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>

namespace llvm::yaml {
    using namespace witness_inject;

    template<> struct ScalarEnumerationTraits<witness::EntryType> {
        static void enumeration(IO &io, witness::EntryType &value) {
            io.enumCase(value, "invariant_set", witness::EntryType::InvariantSet);
        }
    };

    template<> struct ScalarEnumerationTraits<witness::InvariantType> {
        static void enumeration(IO &io, witness::InvariantType &value) {
            io.enumCase(value, "location_invariant", witness::InvariantType::Location);
            io.enumCase(value, "loop_invariant", witness::InvariantType::Loop);
        }
    };

    template<> struct ScalarEnumerationTraits<witness::InvariantFormat> {
        static void enumeration(IO &io, witness::InvariantFormat &value) {
            io.enumCase(value, "c_expression", witness::InvariantFormat::CExpression);
        }
    };

    template<> struct MappingTraits<witness::Location> {
        static void mapping(IO &io, witness::Location &loc) {
            io.mapRequired("file_name", loc.filepath);
            io.mapRequired("line", loc.line);
            io.mapRequired("column", loc.column);
            io.mapRequired("function", loc.function);
        }
    };

    template<> struct MappingTraits<witness::Invariant> {
        static void mapping(IO &io, witness::Invariant &inv) {
            io.mapRequired("type", inv.type);
            io.mapRequired("location", inv.location);
            io.mapRequired("value", inv.value);
            io.mapRequired("format", inv.format);
        }
    };

    template<> struct MappingTraits<witness::InvariantItem> {
        static void mapping(IO &io, witness::InvariantItem &item) {
            io.mapRequired("invariant", item.invariant);
        }
    };

    template<> struct MappingTraits<witness::Entry> {
        static void mapping(IO &io, witness::Entry &doc) {
            io.mapRequired("entry_type", doc.entry_type);
            io.mapRequired("content", doc.content);
        }
    };
}

LLVM_YAML_IS_SEQUENCE_VECTOR(witness_inject::witness::InvariantItem);
LLVM_YAML_IS_SEQUENCE_VECTOR(witness_inject::witness::Entry);

namespace witness_inject {
    void witness::Entry::ReadInto(llvm::StringRef filepath, llvm::MemoryBufferRef bufRef, std::vector<witness::Entry> &out) {
        llvm::yaml::Input yaml(bufRef);
        yaml.setAllowUnknownKeys(true);
        yaml >> out;

        llvm::SmallString<1024> witnessFilePath;
        if (auto ec = llvm::sys::fs::real_path(filepath, witnessFilePath)) {
            throw std::runtime_error{ec.message()};
        }
        for (auto &entry : out) {
            for (auto &inv : entry.content) {
                if (llvm::sys::path::is_absolute(inv.invariant.location.filepath)) {
                    continue;
                }

                llvm::SmallString<1024> filepath = llvm::sys::path::parent_path(witnessFilePath);
                llvm::sys::path::append(filepath, inv.invariant.location.filepath);
                inv.invariant.location.filepath = filepath.c_str();
            }
        }
    }
}
