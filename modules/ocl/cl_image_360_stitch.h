/*
 * cl_image_360_stitch.h - CL Image 360 stitch
 *
 *  Copyright (c) 2016 Intel Corporation
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
 *
 * Author: Wind Yuan <feng.yuan@intel.com>
 */

#ifndef XCAM_CL_IMAGE_360_STITCH_H
#define XCAM_CL_IMAGE_360_STITCH_H

#include <xcam_std.h>
#include <interface/stitcher.h>
#include <interface/feature_match.h>
#include <ocl/cl_multi_image_handler.h>
#include <ocl/cl_fisheye_handler.h>
#include <ocl/cl_blender.h>

namespace XCam {

struct CLFisheyeParams {
    SmartPtr<CLFisheyeHandler>  handler;
    SmartPtr<BufferPool>        pool;
    SmartPtr<VideoBuffer>       buf;

    uint32_t                    width;
    uint32_t                    height;

    CLFisheyeParams () : width (0), height (0) {}
};

class CLImage360Stitch;
class CLBlenderGlobalScaleKernel
    : public CLBlenderScaleKernel
{
public:
    explicit CLBlenderGlobalScaleKernel (
        const SmartPtr<CLContext> &context, SmartPtr<CLImage360Stitch> &stitch, bool is_uv);

protected:
    virtual SmartPtr<CLImage> get_input_image ();
    virtual SmartPtr<CLImage> get_output_image ();
    virtual bool get_output_info (uint32_t &out_width, uint32_t &out_height, int &out_offset_x);

private:
    SmartPtr<CLImage360Stitch>  _stitch;
};

class CLImage360Stitch
    : public CLMultiImageHandler
{
public:
    explicit CLImage360Stitch (
        const SmartPtr<CLContext> &context, CLBlenderScaleMode scale_mode,
        FisheyeDewarpMode dewarp_mode, StitchResMode res_mode, int fisheye_num);

    void set_feature_match (bool enable);

    bool set_stitch_info (StitchInfo stitch_info);
    StitchInfo get_stitch_info ();
    void set_output_size (uint32_t width, uint32_t height) {
        _output_width = width; //XCAM_ALIGN_UP (width, XCAM_BLENDER_ALIGNED_WIDTH);
        _output_height = height;
    }

    bool set_fisheye_handler (SmartPtr<CLFisheyeHandler> fisheye, int index);
    bool set_blender (SmartPtr<CLBlender> blender, int idx);

    bool set_intrinsic_names (const char *intr_names[]);
    bool set_extrinsic_names (const char *extr_names[]);

    const BowlDataConfig &get_fisheye_bowl_config (int index = 0);

    bool set_image_overlap (const int idx, const Rect &overlap0, const Rect &overlap1);
    const Rect &get_image_overlap (int img_idx, int num) {
        XCAM_ASSERT (img_idx < _fisheye_num && num < 2);
        return _overlaps[img_idx][num];
    }

    SmartPtr<VideoBuffer> &get_global_scale_input () {
        return _scale_global_input;
    }
    SmartPtr<VideoBuffer> &get_global_scale_output () {
        return _scale_global_output;
    }

    int get_fisheye_num () {
        return _fisheye_num;
    }

protected:
    virtual XCamReturn prepare_buffer_pool_video_info (const VideoBufferInfo &input, VideoBufferInfo &output);
    virtual XCamReturn prepare_parameters (SmartPtr<VideoBuffer> &input, SmartPtr<VideoBuffer> &output);
    virtual XCamReturn execute_done (SmartPtr<VideoBuffer> &output);

    XCamReturn ensure_fisheye_parameters (SmartPtr<VideoBuffer> &input, SmartPtr<VideoBuffer> &output);
    XCamReturn prepare_local_scale_blender_parameters (
        SmartPtr<VideoBuffer> &input0, SmartPtr<VideoBuffer> &input1, SmartPtr<VideoBuffer> &output, int idx, int idx_next);
    XCamReturn prepare_global_scale_blender_parameters (
        SmartPtr<VideoBuffer> &input0, SmartPtr<VideoBuffer> &input1, SmartPtr<VideoBuffer> &output,
        int idx, int idx_next, int &cur_start_pos);

    bool create_buffer_pool (SmartPtr<BufferPool> &buf_pool, uint32_t width, uint32_t height,
                             uint32_t aligned_width, uint32_t aligned_height);
    XCamReturn get_global_scale_inbuf (const SmartPtr<VideoBuffer> &output, SmartPtr<VideoBuffer> &scale_input);

    virtual XCamReturn sub_handler_execute_done (SmartPtr<CLImageHandler> &handler);

    void update_image_overlap ();

private:
    XCamReturn init_fisheye_info (SmartPtr<VideoBuffer> &output);
    void init_feature_match ();
    void update_scale_factors (uint32_t fm_idx, const Rect &crop_left, const Rect &crop_right);

    void init_sphere_fisheye_params (SmartPtr<VideoBuffer> &output);
    void init_bowl_fisheye_params (SmartPtr<VideoBuffer> &output);

private:
    XCAM_DEAD_COPY (CLImage360Stitch);

private:
    SmartPtr<CLContext>         _context;
    CLFisheyeParams             _fisheye[XCAM_STITCH_FISHEYE_MAX_NUM];
    SmartPtr<CLBlender>         _blender[XCAM_STITCH_FISHEYE_MAX_NUM];

    uint32_t                    _output_width;
    uint32_t                    _output_height;
    ImageMergeInfo              _img_merge_info[XCAM_STITCH_FISHEYE_MAX_NUM];
    Rect                        _overlaps[XCAM_STITCH_FISHEYE_MAX_NUM][2];   // 2=>Overlap0 and overlap1

    CLBlenderScaleMode          _scale_mode;
    SmartPtr<BufferPool>        _scale_buf_pool;
    SmartPtr<VideoBuffer>       _scale_global_input;
    SmartPtr<VideoBuffer>       _scale_global_output;

    FisheyeDewarpMode           _dewarp_mode;
    StitchResMode               _res_mode;

    bool                        _enable_fm;
    SmartPtr<FeatureMatch>      _feature_match[XCAM_STITCH_FISHEYE_MAX_NUM];

    bool                        _is_stitch_inited;
    int                         _fisheye_num;
    StitchInfo                  _stitch_info;

    char                       *_intr_names[XCAM_STITCH_FISHEYE_MAX_NUM];
    char                       *_extr_names[XCAM_STITCH_FISHEYE_MAX_NUM];
};

SmartPtr<CLImageHandler>
create_image_360_stitch (
    const SmartPtr<CLContext> &context,
    bool need_seam = false,
    CLBlenderScaleMode scale_mode = CLBlenderScaleLocal,
    bool fisheye_map = false,
    bool need_lsc = false,
    FisheyeDewarpMode dewarp_mode = DewarpSphere,
    StitchResMode res_mode = StitchRes1080P2Cams,
    int fisheye_num = 2);

}

#endif //XCAM_CL_IMAGE_360_STITCH_H
