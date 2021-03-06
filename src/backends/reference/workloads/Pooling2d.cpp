﻿//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "Pooling2d.hpp"
#include "DataLayoutIndexed.hpp"

#include <armnn/Exceptions.hpp>
#include <armnn/Types.hpp>

#include <boost/numeric/conversion/cast.hpp>

#include <limits>
#include <algorithm>
#include <functional>

namespace
{
    using PoolingAlgorithm = armnn::PoolingAlgorithm;

    float DefaultInitializer(PoolingAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case PoolingAlgorithm::Max:
            {
                return std::numeric_limits<float>::lowest();
            }
            case PoolingAlgorithm::Average:
            case PoolingAlgorithm::L2:
            {
                return 0.0f;
            }
            default:
            {
                throw armnn::InvalidArgumentException("Unsupported pooling algorithm");
            }
        }
    }

    using Accumulator = std::function<void(float & accu, float value)>;

    Accumulator GetAccumulator(PoolingAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case PoolingAlgorithm::Max:
            {
                return [](float & accu, float value) {
                    if (value > accu) {
                        accu = value;
                    }
                };
            }

            case PoolingAlgorithm::Average:
            {
                return [](float & accu, float value) {
                    accu += value;
                };
            }

            case PoolingAlgorithm::L2:
            {
                return [](float & accu, float value) {
                    accu += (value*value);
                };
            }

            default:
            {
                throw armnn::InvalidArgumentException("Unsupported pooling algorithm");
            }
        }
    }

    using Executor = std::function<void(float & accumulated, float kernelSize)>;

    Executor GetExecutor(PoolingAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case PoolingAlgorithm::Max:
            {
                return [](float & accumulated, float kernelSize) {};
            }

            case PoolingAlgorithm::Average:
            {
                return [](float & accumulated, float kernelSize) {
                    accumulated /= kernelSize;
                };
            }

            case PoolingAlgorithm::L2:
            {
                return [](float & accumulated, float kernelSize) {
                    accumulated = sqrtf(accumulated / kernelSize);
                };
            }

            default:
            {
                throw armnn::InvalidArgumentException("Unsupported pooling algorithm");
            }
        }
    }

    bool OnPaddingOnly(int start, int end, int maxRange)
    {
        if (end <= 0 || start > maxRange)
        {
            return true;
        }
        else
        {
            return false;
        }
    }


    bool ClampRange(int & start, int & end, int maxRange)
    {
        if (start < 0 || end > maxRange)
        {
            start = std::min(std::max(start, 0), maxRange);
            end   = std::min(std::max(end, 0), maxRange);
            return true;
        }
        else
        {
            return false;
        }
    }
}

using namespace armnnUtils;

namespace armnn
{
void Pooling2d(Decoder<float>& rInputDecoder,
               Encoder<float>& rOutputEncoder,
               const TensorInfo& inputInfo,
               const TensorInfo& outputInfo,
               const Pooling2dDescriptor& params)
{
    const DataLayoutIndexed dataLayout(params.m_DataLayout);
    auto channelsIndex = dataLayout.GetChannelsIndex();
    auto heightIndex = dataLayout.GetHeightIndex();
    auto widthIndex = dataLayout.GetWidthIndex();

    const int batchSize    = boost::numeric_cast<int>(outputInfo.GetShape()[0]);
    const int channels     = boost::numeric_cast<int>(outputInfo.GetShape()[channelsIndex]);
    const int heightOutput = boost::numeric_cast<int>(outputInfo.GetShape()[heightIndex]);
    const int widthOutput  = boost::numeric_cast<int>(outputInfo.GetShape()[widthIndex]);
    const int heightInput  = boost::numeric_cast<int>(inputInfo.GetShape()[heightIndex]);
    const int widthInput   = boost::numeric_cast<int>(inputInfo.GetShape()[widthIndex]);
    const int padLeft      = boost::numeric_cast<int>(params.m_PadLeft);
    const int padRight     = boost::numeric_cast<int>(params.m_PadRight);
    const int padTop       = boost::numeric_cast<int>(params.m_PadTop);
    const int padBottom    = boost::numeric_cast<int>(params.m_PadBottom);
    const int strideX      = boost::numeric_cast<int>(params.m_StrideX);
    const int strideY      = boost::numeric_cast<int>(params.m_StrideY);
    const int poolHeight   = boost::numeric_cast<int>(params.m_PoolHeight);
    const int poolWidth    = boost::numeric_cast<int>(params.m_PoolWidth);

    float defaultInitializer = DefaultInitializer(params.m_PoolType);

    Accumulator accumulate = GetAccumulator(params.m_PoolType);
    Executor execute       = GetExecutor(params.m_PoolType);

    TensorShape outputShape = outputInfo.GetShape();
    TensorShape inputShape =  inputInfo.GetShape();

    // Check supported padding methods outside the loop to simplify
    // the inner loop.
    if (params.m_PaddingMethod != PaddingMethod::Exclude &&
        params.m_PaddingMethod != PaddingMethod::IgnoreValue)
    {
        throw armnn::InvalidArgumentException("Unsupported padding type");
    }

    for (int n = 0; n < batchSize; n++)
    {
        for (int c = 0; c < channels; c++)
        {
            for (int yOutput = 0; yOutput < heightOutput; yOutput++)
            {
                //  Calculate values independent of the x axis
                int hstart = (yOutput * strideY) - padTop;
                int hend = hstart + poolHeight;
                // Clamp the pooling region inside the valid input area (which includes the padding).
                // This is necessary because the final pooling in a row may overlap beyond the padding.
                hend = std::min(hend, heightInput + padBottom);

                int height = hend - hstart;
                bool hclamped = ClampRange(hstart, hend, heightInput);

                for (int xOutput = 0; xOutput < widthOutput; xOutput++)
                {
                    int wstart = (xOutput * strideX) - padLeft;
                    int wend = wstart + poolWidth;

                    // Clamp the pooling region inside the valid input area (which includes the padding).
                    // This is necessary because the final pooling in a row may overlap beyond the padding.
                    wend = std::min(wend, widthInput + padRight);

                    float result = defaultInitializer;
                    float poolAreaSize = boost::numeric_cast<float>(height * (wend - wstart));

                    // Special case: when the pooling kernel is over a padding region and the padding
                    //               size is larger or equal to the kernel and the kernel only covers
                    //               padding and no real values, then we initialize the result as zero
                    //               by convention. This is because we need to choose a value here and
                    //               all values we have are padding, which we ignore.
                    if (OnPaddingOnly(hstart, hend, heightInput) ||
                        OnPaddingOnly(wstart, wend, widthInput))
                    {
                        result = 0.0f;

                        unsigned int outputIndex = dataLayout.GetIndex(outputShape,
                                                                       boost::numeric_cast<unsigned int>(n),
                                                                       boost::numeric_cast<unsigned int>(c),
                                                                       boost::numeric_cast<unsigned int>(yOutput),
                                                                       boost::numeric_cast<unsigned int>(xOutput));
                        rOutputEncoder[outputIndex];
                        rOutputEncoder.Set(result);
                        continue;
                    }

                    bool clamped = hclamped |= ClampRange(wstart, wend, widthInput);

                    if (clamped && params.m_PaddingMethod == PaddingMethod::Exclude)
                    {
                        // When we exclude the padding, it means we calculate with a smaller
                        // kernel size, so I changed the divisor here.
                        poolAreaSize = boost::numeric_cast<float>((hend - hstart) * (wend - wstart));
                    }

                    for (auto yInput = hstart; yInput < hend; yInput++)
                    {
                        for (auto xInput = wstart; xInput < wend; xInput++)
                        {
                            unsigned int inputIndex = dataLayout.GetIndex(inputShape,
                                                                          boost::numeric_cast<unsigned int>(n),
                                                                          boost::numeric_cast<unsigned int>(c),
                                                                          boost::numeric_cast<unsigned int>(yInput),
                                                                          boost::numeric_cast<unsigned int>(xInput));

                            rInputDecoder[inputIndex];
                            float inval = rInputDecoder.Get();

                            accumulate(result, inval);
                        }
                    }

                    execute(result, poolAreaSize);

                    unsigned int outputIndex = dataLayout.GetIndex(outputShape,
                                                                   boost::numeric_cast<unsigned int>(n),
                                                                   boost::numeric_cast<unsigned int>(c),
                                                                   boost::numeric_cast<unsigned int>(yOutput),
                                                                   boost::numeric_cast<unsigned int>(xOutput));

                    rOutputEncoder[outputIndex];
                    rOutputEncoder.Set(result);
                }
            }
        }
    }
}

} //namespace armnn
