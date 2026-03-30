#ifndef CALC_FREQ_DOMAIN_H_
#define CALC_FREQ_DOMAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"
#include "utils/fft.h"

// ============================================================================
// 频域特征计算模块
// 负责：FFT 分析、功率谱密度、主频分析等
// ============================================================================

// 计算全量包长序列 FFT
int calc_fft_all(flow_feature_state_t* state,
                 fft_result_t* result);

// 计算前向包 FFT
int calc_fft_fwd(flow_feature_state_t* state,
                 fft_result_t* result);

// 计算后向包 FFT
int calc_fft_bwd(flow_feature_state_t* state,
                 fft_result_t* result);

// 计算 FFT 派生特征
void calc_derived_fft(flow_feature_state_t* state,
                      cJSON* output);

// 计算功率谱密度峰值
double calc_psd_peak(fft_result_t* result);

#endif /* CALC_FREQ_DOMAIN_H_ */
