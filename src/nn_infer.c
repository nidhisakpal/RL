#include "nn_infer.h"
#include <stdio.h>
#include <stdlib.h>

nn_model_t* nn_load(const char* model_path, int in_size, int out_size) {
    nn_model_t* m = calloc(1, sizeof(nn_model_t));

    m->api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    m->api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "nn", &m->env);
    m->api->CreateSessionOptions(&m->opts);

    m->api->SetGraphOptimizationLevel(m->opts, ORT_ENABLE_ALL);

    if (m->api->CreateSession(m->env, model_path, m->opts, &m->session) != ORT_OK) {
        fprintf(stderr, "ERROR: could not load ONNX model: %s\n", model_path);
        free(m);
        return NULL;
    }

    m->in_size = in_size;
    m->out_size = out_size;

    return m;
}

int nn_predict(nn_model_t* m, const float* input, float* output) {
    OrtMemoryInfo* meminfo;
    m->api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &meminfo);

    // Input tensor
    int64_t dims[2] = {1, m->in_size};
    OrtValue* inp = NULL;
    m->api->CreateTensorWithDataAsOrtValue(
        meminfo, (void*)input, sizeof(float)*m->in_size,
        dims, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inp
    );

    OrtValue* out = NULL;
    int64_t out_dims[2] = {1, m->out_size};
    m->api->CreateTensorWithDataAsOrtValue(
        meminfo, output, sizeof(float)*m->out_size,
        out_dims, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &out
    );

    const char* in_names[] = {"x"};
    const char* out_names[] = {"alpha"};

    if (m->api->Run(
        m->session, NULL,
        in_names, (const OrtValue* const*)&inp, 1,
        out_names, 1, &out
    ) != ORT_OK) {
        return -1;
    }

    m->api->ReleaseValue(inp);
    m->api->ReleaseValue(out);
    m->api->ReleaseMemoryInfo(meminfo);

    return 0;
}

void nn_unload(nn_model_t* m) {
    if (!m) return;

    m->api->ReleaseSession(m->session);
    m->api->ReleaseSessionOptions(m->opts);
    m->api->ReleaseEnv(m->env);

    free(m);
}
