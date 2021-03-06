//**********************************************************************;
// Copyright (c) 2015-2018, Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of Intel Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//**********************************************************************;

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <tss2/tss2_esys.h>

#include "files.h"
#include "log.h"
#include "tpm2_options.h"
#include "tpm2_tool.h"
#include "tpm2_util.h"

typedef struct tpm_rsaencrypt_ctx tpm_rsaencrypt_ctx;
struct tpm_rsaencrypt_ctx {
    const char *context_arg;
    tpm2_loaded_object key_context;
    TPM2B_PUBLIC_KEY_RSA message;
    char *output_path;
    char *input_path;
};

static tpm_rsaencrypt_ctx ctx = {
    .context_arg = NULL
};

static bool rsa_encrypt_and_save(ESYS_CONTEXT *context) {

    bool ret = true;
    // Inputs
    TPMT_RSA_DECRYPT scheme;
    TPM2B_DATA label;
    // Outputs
    TPM2B_PUBLIC_KEY_RSA *out_data;

    scheme.scheme = TPM2_ALG_RSAES;
    label.size = 0;

    TSS2_RC rval = Esys_RSA_Encrypt(context, ctx.key_context.tr_handle,
                        ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                        &ctx.message, &scheme, &label, &out_data);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_RSA_Encrypt, rval);
        return false;
    }

    if (ctx.output_path) {
        ret = files_save_bytes_to_file(ctx.output_path, out_data->buffer,
                out_data->size);
    }

    tpm2_util_print_tpm2b((TPM2B *)out_data);

    free(out_data);

    return ret;
}

static bool on_option(char key, char *value) {

    switch (key) {
    case 'c':
        ctx.context_arg = value;
        break;
    case 'o':
        ctx.output_path = value;
        break;
    }
    return true;
}

static bool on_args(int argc, char **argv) {

    if (argc > 1) {
        LOG_ERR("Only supports one hash input file, got: %d", argc);
        return false;
    }

    ctx.input_path = argv[0];

    return true;
}

bool tpm2_tool_onstart(tpm2_options **opts) {

    static const struct option topts[] = {
      {"out-file",    required_argument, NULL, 'o'},
      {"key-context", required_argument, NULL, 'c'},
    };

    *opts = tpm2_options_new("o:c:", ARRAY_LEN(topts), topts,
                             on_option, on_args, 0);

    return *opts != NULL;
}

static bool init(ESYS_CONTEXT *context) {

    if (!ctx.context_arg) {
        LOG_ERR("Expected option C");
        return false;
    }

    tpm2_object_load_rc olrc = tpm2_util_object_load(context,
                                ctx.context_arg, &ctx.key_context);
    if (olrc == olrc_error) {
        return false;
    }

    ctx.message.size = BUFFER_SIZE(TPM2B_PUBLIC_KEY_RSA, buffer);
    return files_load_bytes_from_buffer_or_file_or_stdin(NULL,ctx.input_path,
        &ctx.message.size, ctx.message.buffer);
}

int tpm2_tool_onrun(ESYS_CONTEXT *context, tpm2_option_flags flags) {

    UNUSED(flags);

    bool result = init(context);
    if (!result) {
        return 1;
    }

    return rsa_encrypt_and_save(context) != true;
}
