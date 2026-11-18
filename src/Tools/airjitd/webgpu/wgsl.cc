// wgsl.cc - SPIR-V to WGSL with Tint, the shader step between air2spv and WebGPU
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "wgsl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "src/tint/api/tint.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/spirv/reader/reader.h"
#include "src/tint/lang/wgsl/writer/writer.h"

static void fail(char *err, size_t errlen, const std::string &why)
{
    if (err != nullptr && errlen > 0)
        snprintf(err, errlen, "%s", why.c_str());
}

extern "C" char *pd_spv_to_wgsl(const uint32_t *words, size_t nwords, char *err, size_t errlen)
{
    static bool initialized;
    if (!initialized) {
        tint::Initialize();
        initialized = true;
    }

    std::vector<uint32_t> spirv(words, words + nwords);
    tint::spirv::reader::Options readOptions;
    auto module = tint::spirv::reader::ReadIR(spirv, readOptions);
    if (module != tint::Success) {
        fail(err, errlen, "SPIR-V reader: " + module.Failure().reason);
        return nullptr;
    }
    tint::wgsl::writer::Options writeOptions;
    auto output = tint::wgsl::writer::WgslFromIR(module.Get(), writeOptions);
    if (output != tint::Success) {
        fail(err, errlen, "WGSL writer: " + output.Failure().reason);
        return nullptr;
    }
    const std::string &wgsl = output->wgsl;
    char *text = static_cast<char *>(malloc(wgsl.size() + 1));
    if (text != nullptr)
        memcpy(text, wgsl.c_str(), wgsl.size() + 1);
    return text;
}
