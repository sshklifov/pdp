#pragma once

#include "chunk_array.h"
#include "data/arena.h"
#include "data/stack.h"
#include "mi_expr.h"
#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

struct RpcPass {
  RpcPass(const StringSlice &s);

  bool Parse();

 private:
  inline uint8_t ReadBigEndian8(const uint8_t *p) { return uint8_t(p[0]); }

  inline int8_t ReadBigEndianS8(const uint8_t *p) { return int8_t(p[0]); }

  inline uint16_t ReadBigEndian16(const uint8_t *p) {
    return (uint16_t(p[0]) << 8) | (uint16_t(p[1]));
  }

  inline uint32_t ReadBigEndian32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) |
           (uint32_t(p[3]));
  }

  inline uint64_t ReadBigEndian64(const uint8_t *p) {
    return (uint64_t(p[0]) << 56) | (uint64_t(p[1]) << 48) | (uint64_t(p[2]) << 40) |
           (uint64_t(p[3]) << 32) | (uint64_t(p[4]) << 24) | (uint64_t(p[5]) << 16) |
           (uint64_t(p[6]) << 8) | (uint64_t(p[7]));
  }

  inline int16_t ReadBigEndianS16(const uint8_t *p) {
    return (int16_t(p[0]) << 8) | (int16_t(p[1]));
  }

  inline int32_t ReadBigEndianS32(const uint8_t *p) {
    return (int32_t(p[0]) << 24) | (int32_t(p[1]) << 16) | (int32_t(p[2]) << 8) | (int32_t(p[3]));
  }

  inline int64_t ReadBigEndianS64(const uint8_t *p) {
    return (int64_t(p[0]) << 56) | (int64_t(p[1]) << 48) | (int64_t(p[2]) << 40) |
           (int64_t(p[3]) << 32) | (int64_t(p[4]) << 24) | (int64_t(p[5]) << 16) |
           (int64_t(p[6]) << 8) | (int64_t(p[7]));
  }

  // MiExprBase *CreateExprInt(uint8_t bytes, bool sign, unsigned char *stream) {
  //   MiExprInt *expr = static_cast<MiExprInt *>(
  //       chunk_array.AllocateUnchecked(sizeof(MiExprBase) + sizeof(uint64_t)));
  //   expr->kind = MiExprBase::kInt;

  //   head += 1;
  //   expr->value = ReadBigEndian8(p);
  //   head += 1;
  //   return expr;
  // }

  MiExprBase *ParseInteger() {
    MiExprInt *expr = static_cast<MiExprInt *>(
        chunk_array.AllocateUnchecked(sizeof(MiExprBase) + sizeof(uint64_t)));
    expr->kind = MiExprBase::kInt;
    // uint8_t 
    head += 1;
    switch (byte) {
      case 0xcc:
        expr->value = ReadBigEndian8(head);
        head += 1;
        return expr;
      case 0xd0:
        expr->value = ReadBigEndianS8(head);
        head += 1;
        return expr;
      case 0xcd:
        expr->value = ReadBigEndian16(head);
        head += 2;
        return expr;
      case 0xd1:
        expr->value = ReadBigEndianS16(head);
        head += 2;
        return expr;
      case 0xce:
        expr->value = ReadBigEndian32(head);
        head += 4;
        return expr;
      case 0xd2:
        expr->value = ReadBigEndianS32(head);
        head += 4;
        return expr;
      case 0xcf:
        expr->value = ReadBigEndian64(head);
        head += 8;
        return expr;
      case 0xd3:
        expr->value = ReadBigEndianS64(head);
        head += 8;
        return expr;
      default:
        PDP_UNREACHABLE();
    }
  }

    MiExprBase *BigSwitch(unsigned char byte) {
      unsigned char *head = nullptr;
      switch (byte) {
          // 8bit signed
        case 0xd0:
          // 16bit signed
        case 0xd1:
          // 32bit signed
        case 0xd2:
          // 64bit signed
        case 0xd3:
          // 8bit unsigned
        case 0xcc:
          // 16bit unsigned
        case 0xcd:
          // 32bit unsigned
        case 0xce:
          // 64bit unsigned
        case 0xcf: {
          MiExprInt *expr = static_cast<MiExprInt *>(
              chunk_array.AllocateUnchecked(sizeof(MiExprBase) + sizeof(uint64_t)));
          expr->kind = MiExprBase::kInt;
          head += 1;
          switch (byte) {
            case 0xcc:
              expr->value = ReadBigEndian8(head);
              head += 1;
              return expr;
            case 0xd0:
              expr->value = ReadBigEndianS8(head);
              head += 1;
              return expr;
            case 0xcd:
              expr->value = ReadBigEndian16(head);
              head += 2;
              return expr;
            case 0xd1:
              expr->value = ReadBigEndianS16(head);
              head += 2;
              return expr;
            case 0xce:
              expr->value = ReadBigEndian32(head);
              head += 4;
              return expr;
            case 0xd2:
              expr->value = ReadBigEndianS32(head);
              head += 4;
              return expr;
            case 0xcf:
              expr->value = ReadBigEndian64(head);
              head += 8;
              return expr;
            case 0xd3:
              expr->value = ReadBigEndianS64(head);
              head += 8;
              return expr;
            default:
              PDP_UNREACHABLE();
          }
        }
          // null
        case 0xc0: {
          MiExprBase *expr =
              static_cast<MiExprBase *>(chunk_array.AllocateUnchecked(sizeof(MiExprBase)));
          expr->kind = MiExprBase::kNull;
          head += 1;
          return expr;
        }

        case 0xc2:
        case 0xc3: {
          MiExprInt *expr = static_cast<MiExprInt *>(
              chunk_array.AllocateUnchecked(sizeof(MiExprBase) + sizeof(uint64_t)));
          expr->kind = MiExprBase::kInt;
          expr->value = byte & 0x1;
          head += 1;
          return expr;
        }

          // String with 1 byte length
        case 0xd9:
          // String with 2 byte length
        case 0xda:
          // String with 4 byte length
        case 0xdb: {
          head += 1;
          uint32_t length = 0;
          switch (byte) {
            case 0xd9:
              length = ReadBigEndian8(head);
              head += 1;
              break;
            case 0xda:
              length = ReadBigEndian16(head);
              head += 2;
              break;
            case 0xdb:
              length = ReadBigEndian16(head);
              head += 2;
              break;
            default:
              PDP_UNREACHABLE();
          }
          MiExprString *expr =
              static_cast<MiExprString *>(chunk_array.Allocate(sizeof(MiExprBase) + length));
          expr->kind = MiExprBase::kString;
          expr->size = length;
          // TODO memcopy recoverable.
          return expr;
        }
      }
#if 0
      case 0xdc:
      case 0xde:
        // array : map
        head++;
        // head += ReadBigEndian16(head);
        break;
      case 0xdd:
      case 0xdf:
        // array 32
        head++;
        // head += ReadBigEndian32(head);
        break;
      // case
#endif
      // 00-7f -> int +1
      // eo-ff -> int + 1
      // a0-bf -> length = byte & 0xa0
      // 90-9f -> array (count = byte & 0xf)
      // 80-8f -> fixmap (count = byte & 0xf)
      return nullptr;
    }

    uint8_t *stream;
    uint8_t *end;
    ChunkArray<DefaultAllocator> chunk_array;
  };

#if 0
struct RpcSecondPass {
  RpcSecondPass(const StringSlice &s, RpcFirstPass &first_pass);

  MiExprBase *Parse();

 private:
  MiExprBase *ReportError(const StringSlice &msg);
  MiExprBase *CreateListOrTuple();

  MiExprBase *ParseResult();
  MiExprBase *ParseValue();
  MiExprBase *ParseString();
  MiExprBase *ParseListOrTuple();
  MiExprBase *ParseResultOrValue();

  struct MiRecord {
    MiExprBase *expr;
    union {
      MiExprBase **list_members;
      MiExprTuple::Result *tuple_members;
    };
    char *string_table_ptr;
    uint32_t *hash_table_ptr;
#ifdef PDP_ENABLE_ASSERT
    void *record_end;
#endif
  };

  StringSlice input;
  Stack<RpcFirstPass::MiRecord> first_pass_stack;
  size_t first_pass_marker;
  Arena<DefaultAllocator> arena;
  Stack<MiRecord> second_pass_stack;
};
#endif

}  // namespace pdp
