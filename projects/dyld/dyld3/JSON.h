/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */



#ifndef __JSON_H__
#define __JSON_H__

#include <string.h>

#include <string>
#include <map>
#include <sstream>
#include <vector>

namespace dyld3 {
namespace json {

enum class NodeValueType {
    Default,
    String,
    RawValue,
};

struct Node
{
    NodeValueType               type = NodeValueType::Default;
    std::string                 value;
    std::map<std::string, Node> map;
    std::vector<Node>           array;

    inline Node()
    : type(NodeValueType::Default), value(), map(), array() { }

    inline Node(std::string string)
    : type(NodeValueType::String), value(string), map(), array() { }

    inline Node(const char *string) : Node(std::string{string}) { }

    inline Node(bool b)
    : type(NodeValueType::RawValue), value(b ? "true" : "false")
    , map(), array() { }

    inline Node(int64_t i64)
    : type(NodeValueType::RawValue), value(), map(), array()
    {
        std::ostringstream os{};
        os << i64;
        value = os.str();
    }

    inline Node(uint64_t u64)
    : type(NodeValueType::RawValue), value(), map(), array()
    {
        std::ostringstream os{};
        os << u64;
        value = os.str();
    }
};

static inline Node makeNode(std::string value) {
    Node node;
    node.value = value;
    return node;
}

} // namespace json
} // namespace dyld3


#endif // __JSON_H__
