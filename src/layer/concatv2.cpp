// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "concatv2.h"

namespace ncnn {

DEFINE_LAYER_CREATOR(ConcatV2)

ConcatV2::ConcatV2()
{
    one_blob_only = false;
    support_inplace = false;
}

#if NCNN_STDIO
#if NCNN_STRING
int ConcatV2::load_param(FILE* paramfp)
{
    int nscan = fscanf(paramfp, "%d", &axis);
    if (nscan != 1)
    {
        fprintf(stderr, "ConcatV2 load_param failed %d\n", nscan);
        return -1;
    }

    return 0;
}
#endif // NCNN_STRING
int ConcatV2::load_param_bin(FILE* paramfp)
{
    fread(&axis, sizeof(int), 1, paramfp);

    return 0;
}
#endif // NCNN_STDIO

int ConcatV2::load_param(const unsigned char*& mem)
{
    axis = *(int*)(mem);
    mem += 4;

    return 0;
}

int ConcatV2::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs) const
{
    int dims = bottom_blobs[0].dims;

    if (dims == 1) // axis == 0
    {
        // concat vector
        // total length
        int top_w = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_w += bottom_blob.w;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(top_w);
        if (top_blob.empty())
            return -100;

        float* outptr = top_blob;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];

            int w = bottom_blob.w;

            const float* ptr = bottom_blob;
            for (int i=0; i<w; i++)
            {
                outptr[i] = ptr[i];
            }

            outptr += w;
        }

        return 0;
    }

    if (dims == 2 && axis == 0)
    {
        // concat image
        int w = bottom_blobs[0].w;

        // total height
        int top_h = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_h += bottom_blob.h;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, top_h);
        if (top_blob.empty())
            return -100;

        float* outptr = top_blob;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];

            int size = w * bottom_blob.h;

            const float* ptr = bottom_blob;
            for (int i=0; i<size; i++)
            {
                outptr[i] = ptr[i];
            }

            outptr += size;
        }

        return 0;
    }

    if (dims == 2 && axis == 1)
    {
        // interleave image row
        int h = bottom_blobs[0].h;

        // total width
        int top_w = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_w += bottom_blob.w;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(top_w, h);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for
        for (int i=0; i<h; i++)
        {
            float* outptr = top_blob.row(i);
            for (size_t b=0; b<bottom_blobs.size(); b++)
            {
                const Mat& bottom_blob = bottom_blobs[b];

                const float* ptr = bottom_blob.row(i);
                for (int j=0; j<bottom_blob.w; j++)
                {
                    outptr[j] = ptr[j];
                }

                outptr += bottom_blob.w;
            }
        }

        return 0;
    }

    if (dims == 3 && axis == 0)
    {
        // concat dim
        int w = bottom_blobs[0].w;
        int h = bottom_blobs[0].h;

        // total channels
        int top_channels = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_channels += bottom_blob.c;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, h, top_channels);
        if (top_blob.empty())
            return -100;

        int q = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];

            int channels = bottom_blob.c;
            int size = bottom_blob.cstep * channels;

            const float* ptr = bottom_blob;
            float* outptr = top_blob.channel(q);
            for (int i=0; i<size; i++)
            {
                outptr[i] = ptr[i];
            }

            q += channels;
        }

        return 0;
    }

    if (dims == 3 && axis == 1)
    {
        // interleave dim height
        int w = bottom_blobs[0].w;
        int channels = bottom_blobs[0].c;

        // total height
        int top_h = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_h += bottom_blob.h;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, top_h, channels);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for
        for (int q=0; q<channels; q++)
        {
            float* outptr = top_blob.channel(q);

            for (size_t b=0; b<bottom_blobs.size(); b++)
            {
                const Mat& bottom_blob = bottom_blobs[b];

                int size = bottom_blob.w * bottom_blob.h;

                const float* ptr = bottom_blob.channel(q);
                for (int i=0; i<size; i++)
                {
                    outptr[i] = ptr[i];
                }
            }
        }

        return 0;
    }

    if (dims == 3 && axis == 2)
    {
        // interleave dim width
        int h = bottom_blobs[0].h;
        int channels = bottom_blobs[0].c;

        // total height
        int top_w = 0;
        for (size_t b=0; b<bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_w += bottom_blob.w;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(top_w, h, channels);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for
        for (int q=0; q<channels; q++)
        {
            float* outptr = top_blob.channel(q);

            for (int i=0; i<h; i++)
            {
                for (size_t b=0; b<bottom_blobs.size(); b++)
                {
                    const Mat& bottom_blob = bottom_blobs[b];

                    const float* ptr = bottom_blob.channel(q).row(i);
                    for (int j=0; j<bottom_blob.w; j++)
                    {
                        outptr[j] = ptr[j];
                    }

                    outptr += bottom_blob.w;
                }
            }
        }

        return 0;
    }

    return 0;
}

} // namespace ncnn
