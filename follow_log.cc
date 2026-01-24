#include <fcntl.h>
#define PACKAGE_VERSION
#include <bfd.h>

#include <cxxabi.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <cstring>

#include "data/vector.h"
#include "external/emhash8.h"
#include "strings/dynamic_string.h"
#include "strings/rolling_buffer.h"
#include "strings/string_builder.h"

void WriteSlice(const pdp::StringSlice &str) {
  pdp::WriteFully(STDOUT_FILENO, str.Data(), str.Size());
}

void WriteFileError(const char *fmt, const char *filename) {
  pdp::StringBuilder builder;
  builder.AppendFormat(fmt, pdp::StringSlice(filename));
  WriteSlice(builder.ToSlice());
}

class FileSymbolResolver : public pdp::NonCopyableNonMovable {
  struct SourceLine {
    const char *filename;
    const char *func;
    uint32_t line;
    uint32_t discriminator;
    uint32_t is_inlined;
  };

 public:
  FileSymbolResolver(const char *filename, int max_function_length, bool enable_inlining)
      : max_function_length(max_function_length), enable_inlining(enable_inlining) {
    err = nullptr;
    syms = nullptr;
    handle = bfd_openr(filename, NULL);
    if (!handle) {
      err = "Failed to open {}!\n";
      return;
    }
    handle->flags |= BFD_DECOMPRESS;

    if (!bfd_check_format(handle, bfd_object)) {
      err = "Invalid file format of {}!\n";
      return;
    }
    bool has_syms = bfd_get_file_flags(handle) & HAS_SYMS;
    if (!has_syms) {
      err = "No symbols in {} (did you compile with -g1)?\n";
      return;
    }

    bool dynamic = false;
    long symtab_bytes = bfd_get_symtab_upper_bound(handle);
    if (symtab_bytes == 0) {
      symtab_bytes = bfd_get_dynamic_symtab_upper_bound(handle);
      dynamic = true;
    }
    if (symtab_bytes <= 0) {
      err = "No symbols in {} (unknown upper bound)!\n";
      return;
    }
    syms = ((asymbol **)malloc(symtab_bytes));
    long symcount = 0;
    if (dynamic) {
      symcount = bfd_canonicalize_dynamic_symtab(handle, syms);
    } else {
      symcount = bfd_canonicalize_symtab(handle, syms);
    }
    if (symcount <= 0) {
      err = "No symbols in {} (failed to canonicalize)!\n";
      return;
    }

    sects.ReserveFor(handle->section_count);
    for (bfd_section *sect = handle->sections; sect != NULL; sect = sect->next) {
      bool allocated = bfd_section_flags(sect) & SEC_ALLOC;
      if (allocated) {
        sects += sect;
      }
    }
  }

  ~FileSymbolResolver() {
    free(syms);
    bfd_close(handle);
  }

  const pdp::Vector<SourceLine> &Resolve(const pdp::StringSlice &addr) {
    bfd_vma pc = bfd_scan_vma(addr.Begin(), NULL, 16);
    return FindSourceLines(pc);
  }

  void Format(const SourceLine &s, pdp::StringBuilder<> &out) {
    if (s.is_inlined) {
      out.Append("(inlined by) ");
    }

    if (s.func) {
      int status = -1;
      char *alloc = abi::__cxa_demangle(s.func, NULL, NULL, &status);
      if (alloc && status == 0) {
        pdp::StringSlice demangled(alloc);
        const auto length =
            demangled.Size() < max_function_length ? demangled.Size() : max_function_length;
        out.Append(demangled.GetLeft(length));
      } else {
        pdp::StringSlice mangled(s.func);
        const auto length =
            mangled.Size() < max_function_length ? mangled.Size() : max_function_length;
        out.Append(mangled.GetLeft(length));
      }
      out.Append(' ');
      free(alloc);
    }

    out.Append("at ");
    if (s.filename) {
      out.Append(pdp::GetBasename(s.filename));
    } else {
      out.Append("??");
    }
    out.Append(':');
    if (s.line > 0) {
      out.Append(s.line);
    } else {
      out.Append("?");
    }
  }

  bool HasErrors() const { return err; }

  void ShowErrors(const char *filename) const {
    if (err) {
      WriteFileError(err, filename);
    }
  }

 private:
  const pdp::Vector<SourceLine> &FindSourceLines(bfd_vma addr) {
    auto it = cache.Find(addr);
    if (it != cache.End()) {
      return it->value;
    }

    pdp::Vector<SourceLine> result;
    for (bfd_section **sect = sects.Begin(); sect < sects.End(); ++sect) {
      bfd_vma start = bfd_section_vma(*sect);
      bfd_vma end = start + bfd_section_size(*sect);
      if (addr >= start && addr < end) {
        bfd_vma off = addr - start;
        SourceLine s;
        unsigned discriminator;
        bool found = bfd_find_nearest_line_discriminator(handle, *sect, syms, off, &s.filename,
                                                         &s.func, &s.line, &discriminator);
        s.is_inlined = false;
        while (found) {
          result += s;
          if (enable_inlining) {
            found = bfd_find_inliner_info(handle, &s.filename, &s.func, &s.line);
            s.is_inlined = true;
          } else {
            found = false;
          }
        }
        if (!result.Empty()) {
          break;
        }
      }
    }
    it = cache.Emplace(addr, std::move(result));
    return it->value;
  }

  const size_t max_function_length;
  const bool enable_inlining;

  bfd *handle;
  asymbol **syms;
  pdp::Vector<bfd_section *> sects;
  const char *err;
  emhash8::Map<long, pdp::Vector<SourceLine>> cache;
};

template <>
struct pdp::CanReallocate<FileSymbolResolver> : std::true_type {};

struct ExecutableAndAddress {
  pdp::DynamicString executable;
  pdp::DynamicString addr;
};

ExecutableAndAddress SplitExecutableAndAddress(char *begin, char *end) {
  if (PDP_UNLIKELY(*end != '\n')) {
    WriteSlice("You messed up the new lines again...\n");
    abort();
  }

  char *close_bracket = end - 1;
  if (close_bracket <= begin || *close_bracket != ')' || (*begin != '.' && *begin != '/')) {
    return {};
  }
  char *it = close_bracket - 1;
  while ((*it >= '0' && *it <= '9') || (*it >= 'a' && *it <= 'f') || (*it >= 'A' && *it <= 'F')) {
    --it;
  }
  if (*it != 'x' && *it != 'X') {
    return {};
  }
  --it;
  if (*it != '0') {
    return {};
  }
  --it;
  if (*it != '-' && *it != '+') {
    return {};
  }
  --it;
  if (*it != '(') {
    return {};
  }

  *it = '\0';
  if (access(begin, X_OK) != 0) {
    return {};
  }
  *it = '(';

  pdp::DynamicString exe(begin, it);
  pdp::DynamicString sym(it + 2, close_bracket);
  return ExecutableAndAddress{std::move(exe), std::move(sym)};
}

void ShowResolverInfo(const emhash8::Map<pdp::DynamicString, FileSymbolResolver> &resolver_map) {
  if (!resolver_map.Empty()) {
    WriteSlice("\n");
    WriteSlice("\e[32m\e[1m================ LOADED ================\e[0m\n");
    bool has_errors = 0;
    for (auto it = resolver_map.Begin(); it < resolver_map.End(); ++it) {
      if (it->value.HasErrors()) {
        const bool is_libc = strstr(it->key.Data(), "libc.so");
        has_errors = !is_libc;
      } else {
        WriteSlice(it->key.ToSlice());
        WriteSlice("\n");
      }
    }
    if (has_errors) {
      WriteSlice("\n");
      WriteSlice("\e[31m\e[1m================ ERRORS ================\e[0m\n");
      for (auto it = resolver_map.Begin(); it < resolver_map.End(); ++it) {
        const bool is_libc = strstr(it->key.Data(), "libc.so");
        if (!is_libc) {
          it->value.ShowErrors(it->key.Data());
        }
      }
    }
  }
}

bool RedirectInput() {
  int fd = open("/home/stef/Downloads/output.txt", O_RDONLY);
  if (fd < 0) {
    return false;
  }

  if (dup2(fd, STDIN_FILENO) < 0) {
    close(fd);
    return false;
  }
  close(fd);
  return true;
}

int main() {
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);

  if (bfd_init() != BFD_INIT_MAGIC) {
    WriteSlice("Tool is compiled with a different libbfd version!\n");
    return 1;
  }

  const int max_function_length = 120;
  const bool enable_inlining = true;
  emhash8::Map<pdp::DynamicString, FileSymbolResolver> resolver_map;

  int fd = open(PDP_LOG_PATH, O_RDONLY, 0644);
  if (PDP_UNLIKELY(fd < 0)) {
    pdp_error("Failed to open {}!", pdp::StringSlice(PDP_LOG_PATH));
    return 1;
  }

  pdp::RollingBuffer input;
  input.SetDescriptor(fd);

  while (true) {
    pdp::MutableLine line = input.ReadLine();
    if (line.Empty()) {
      if (pdp::LockLogFile(input.GetDescriptor())) {
        return 0;
      } else {
        input.WaitForLine(pdp::Milliseconds(1000));
        continue;
      }
    }

    auto [exe, addr] = SplitExecutableAndAddress(line.Begin(), line.End() - 1);
    if (!exe.Empty() && !addr.Empty()) {
      auto it = resolver_map.Find(exe);
      if (it == resolver_map.End()) {
        const char *hm = exe.Data();
        it = resolver_map.Emplace(std::move(exe), hm, max_function_length, enable_inlining);
      }
      const auto &source_lines = it->value.Resolve(addr.ToSlice());
      pdp::StringBuilder builder;
      for (size_t i = 0; i < source_lines.Size(); ++i) {
        it->value.Format(source_lines[i], builder);
        builder.Append("\n");
      }
      if (!source_lines.Empty()) {
        WriteSlice(builder.ToSlice());
      } else {
        WriteSlice(line.ToSlice());
      }
    } else {
      WriteSlice(line.ToSlice());
    }
  }

  return 0;
}
