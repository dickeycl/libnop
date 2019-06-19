/*
 * Copyright 2017 The Native Object Protocols Authors
 * Copyright 2019 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBNOP_INCLUDE_NOP_BASE_LIST_H_
#define LIBNOP_INCLUDE_NOP_BASE_LIST_H_

#include <numeric>
#include <list>

#include <nop/base/encoding.h>
#include <nop/base/utility.h>

namespace nop {

//
// std::list<T> encoding format for non-integral types:
//
// +-----+---------+-----//-----+
// | ARY | INT64:N | N ELEMENTS |
// +-----+---------+-----//-----+
//
// Elements must be valid encodings of type T.
//
// std::list<T> encoding format for integral types:
//
// +-----+---------+---//----+
// | BIN | INT64:L | L BYTES |
// +-----+---------+---//----+
//
// Where L = N * sizeof(T).
//
// Elements are stored as direct little-endian representation of the integral
// value; each element is sizeof(T) bytes in size.
//

// Specialization for non-integral types.
template <typename T, typename Allocator>
struct Encoding<std::list<T, Allocator>, EnableIfNotIntegral<T>>
    : EncodingIO<std::list<T, Allocator>> {
  using Type = std::list<T, Allocator>;

  static constexpr EncodingByte Prefix(const Type& /*value*/) {
    return EncodingByte::Array;
  }

  static constexpr std::size_t Size(const Type& value) {
    return BaseEncodingSize(Prefix(value)) +
           Encoding<SizeType>::Size(value.size()) +
           std::accumulate(value.cbegin(), value.cend(), 0U,
                           [](const std::size_t& sum, const T& element) {
                             return sum + Encoding<T>::Size(element);
                           });
  }

  static constexpr bool Match(EncodingByte prefix) {
    return prefix == EncodingByte::Array;
  }

  template <typename Writer>
  static constexpr Status<void> WritePayload(EncodingByte /*prefix*/,
                                             const Type& value,
                                             Writer* writer) {
    auto status = Encoding<SizeType>::Write(value.size(), writer);
    if (!status)
      return status;

    for (const T& element : value) {
      status = Encoding<T>::Write(element, writer);
      if (!status)
        return status;
    }

    return {};
  }

  template <typename Reader>
  static constexpr Status<void> ReadPayload(EncodingByte /*prefix*/,
                                            Type* value, Reader* reader) {
    SizeType size = 0;
    auto status = Encoding<SizeType>::Read(&size, reader);
    if (!status)
      return status;

    // Clear the list to make sure elements are inserted in the correct order.
    value->clear();
    for (SizeType i = 0; i < size; i++) {
      T element;
      status = Encoding<T>::Read(&element, reader);
      if (!status)
        return status;

      value->push_back(std::move(element));
    }

    return {};
  }
};

// Specialization for integral types.
template <typename T, typename Allocator>
struct Encoding<std::list<T, Allocator>, EnableIfIntegral<T>>
    : EncodingIO<std::list<T, Allocator>> {
  using Type = std::list<T, Allocator>;

  static constexpr EncodingByte Prefix(const Type& /*value*/) {
    return EncodingByte::Binary;
  }

  static constexpr std::size_t Size(const Type& value) {
    const SizeType size = value.size() * sizeof(T);
    return BaseEncodingSize(Prefix(value)) + Encoding<SizeType>::Size(size) +
           size;
  }

  static constexpr bool Match(EncodingByte prefix) {
    return prefix == EncodingByte::Binary;
  }

  template <typename Writer>
  static constexpr Status<void> WritePayload(EncodingByte /*prefix*/,
                                             const Type& value,
                                             Writer* writer) {
    const SizeType length = value.size();
    const SizeType length_bytes = length * sizeof(T);
    auto status = Encoding<SizeType>::Write(length_bytes, writer);
    if (!status)
      return status;

    for (const T& element : value) {
      status = writer->Write(&element, &element + 1);
      if (!status)
        return status;
    }

    return {};
  }

  template <typename Reader>
  static constexpr Status<void> ReadPayload(EncodingByte /*prefix*/,
                                            Type* value, Reader* reader) {
    SizeType size = 0;
    auto status = Encoding<SizeType>::Read(&size, reader);
    if (!status)
      return status;

    if (size % sizeof(T) != 0)
      return ErrorStatus::InvalidContainerLength;

    const SizeType length = size / sizeof(T);

    // Make sure the reader has enough data to fulfill the requested size as a
    // defense against abusive or erroneous binary container sizes.
    status = reader->Ensure(length);
    if (!status)
      return status;

    // Clear the list to make sure elements are inserted in the correct order.
    value->clear();
    for (SizeType i = 0; i < length; i++) {
      T element;
      status = reader->Read(&element, &element + 1);
      if (!status)
        return status;

      value->push_back(std::move(element));
    }

    return {};
  }
};

}  // namespace nop

#endif  // LIBNOP_INCLUDE_NOP_BASE_LIST_H_
