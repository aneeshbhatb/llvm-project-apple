//===-- Decompressor.cpp --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Object/Decompressor.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::support::endian;
using namespace object;

Expected<Decompressor> Decompressor::create(StringRef Name, StringRef Data,
                                            bool IsLE, bool Is64Bit) {
  if (!compression::zlib::isAvailable())
    return createError("zlib is not available");

  Decompressor D(Data);
  if (Error Err = D.consumeCompressedZLibHeader(Is64Bit, IsLE))
    return std::move(Err);
  return D;
}

Decompressor::Decompressor(StringRef Data)
    : SectionData(Data), DecompressedSize(0) {}

Error Decompressor::consumeCompressedZLibHeader(bool Is64Bit,
                                                bool IsLittleEndian) {
  using namespace ELF;
  uint64_t HdrSize = Is64Bit ? sizeof(Elf64_Chdr) : sizeof(Elf32_Chdr);
  if (SectionData.size() < HdrSize)
    return createError("corrupted compressed section header");

  DataExtractor Extractor(SectionData, IsLittleEndian, 0);
  uint64_t Offset = 0;
  if (Extractor.getUnsigned(&Offset, Is64Bit ? sizeof(Elf64_Word)
                                             : sizeof(Elf32_Word)) !=
      ELFCOMPRESS_ZLIB)
    return createError("unsupported compression type");

  // Skip Elf64_Chdr::ch_reserved field.
  if (Is64Bit)
    Offset += sizeof(Elf64_Word);

  DecompressedSize = Extractor.getUnsigned(
      &Offset, Is64Bit ? sizeof(Elf64_Xword) : sizeof(Elf32_Word));
  SectionData = SectionData.substr(HdrSize);
  return Error::success();
}

Error Decompressor::decompress(MutableArrayRef<uint8_t> Buffer) {
  size_t Size = Buffer.size();
  return compression::zlib::decompress(arrayRefFromStringRef(SectionData),
                                       Buffer.data(), Size);
}
