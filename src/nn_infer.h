#ifndef NN_INFER_H
#define NN_INFER_H

#include <onnxruntime_c_api.h>

typedef struct {
    const OrtApi* api;
    OrtEnv* env;
    OrtSession* session;
    OrtSessionOptions* opts;
    int in_size;
    int out_size;
} nn_model_t;

nn_model_t* nn_load(const char* model_path, int in_size, int out_size);
int nn_predict(nn_model_t* m, const float* input, float* output);
void nn_unload(nn_model_t* m);

#endif
