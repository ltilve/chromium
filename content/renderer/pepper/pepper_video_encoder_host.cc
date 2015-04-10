// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_math.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/media/gpu_video_accelerator_util.h"
#include "content/common/pepper_file_util.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/gfx_conversion.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_video_encoder_host.h"
#include "content/renderer/pepper/video_encoder_shim.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

using ppapi::proxy::SerializedHandle;

namespace content {

namespace {

const uint32_t kDefaultNumberOfBitstreamBuffers = 4;

int32_t PP_FromMediaEncodeAcceleratorError(
    media::VideoEncodeAccelerator::Error error) {
  switch (error) {
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      return PP_ERROR_MALFORMED_INPUT;
    case media::VideoEncodeAccelerator::kIllegalStateError:
    case media::VideoEncodeAccelerator::kPlatformFailureError:
      return PP_ERROR_RESOURCE_FAILED;
    // No default case, to catch unhandled enum values.
  }
  return PP_ERROR_FAILED;
}

// TODO(llandwerlin): move following to media_conversion.cc/h?
media::VideoCodecProfile PP_ToMediaVideoProfile(PP_VideoProfile profile) {
  switch (profile) {
    case PP_VIDEOPROFILE_H264BASELINE:
      return media::H264PROFILE_BASELINE;
    case PP_VIDEOPROFILE_H264MAIN:
      return media::H264PROFILE_MAIN;
    case PP_VIDEOPROFILE_H264EXTENDED:
      return media::H264PROFILE_EXTENDED;
    case PP_VIDEOPROFILE_H264HIGH:
      return media::H264PROFILE_HIGH;
    case PP_VIDEOPROFILE_H264HIGH10PROFILE:
      return media::H264PROFILE_HIGH10PROFILE;
    case PP_VIDEOPROFILE_H264HIGH422PROFILE:
      return media::H264PROFILE_HIGH422PROFILE;
    case PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE:
      return media::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case PP_VIDEOPROFILE_H264SCALABLEBASELINE:
      return media::H264PROFILE_SCALABLEBASELINE;
    case PP_VIDEOPROFILE_H264SCALABLEHIGH:
      return media::H264PROFILE_SCALABLEHIGH;
    case PP_VIDEOPROFILE_H264STEREOHIGH:
      return media::H264PROFILE_STEREOHIGH;
    case PP_VIDEOPROFILE_H264MULTIVIEWHIGH:
      return media::H264PROFILE_MULTIVIEWHIGH;
    case PP_VIDEOPROFILE_VP8_ANY:
      return media::VP8PROFILE_ANY;
    case PP_VIDEOPROFILE_VP9_ANY:
      return media::VP9PROFILE_ANY;
    // No default case, to catch unhandled PP_VideoProfile values.
  }
  return media::VIDEO_CODEC_PROFILE_UNKNOWN;
}

PP_VideoProfile PP_FromMediaVideoProfile(media::VideoCodecProfile profile) {
  switch (profile) {
    case media::H264PROFILE_BASELINE:
      return PP_VIDEOPROFILE_H264BASELINE;
    case media::H264PROFILE_MAIN:
      return PP_VIDEOPROFILE_H264MAIN;
    case media::H264PROFILE_EXTENDED:
      return PP_VIDEOPROFILE_H264EXTENDED;
    case media::H264PROFILE_HIGH:
      return PP_VIDEOPROFILE_H264HIGH;
    case media::H264PROFILE_HIGH10PROFILE:
      return PP_VIDEOPROFILE_H264HIGH10PROFILE;
    case media::H264PROFILE_HIGH422PROFILE:
      return PP_VIDEOPROFILE_H264HIGH422PROFILE;
    case media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE;
    case media::H264PROFILE_SCALABLEBASELINE:
      return PP_VIDEOPROFILE_H264SCALABLEBASELINE;
    case media::H264PROFILE_SCALABLEHIGH:
      return PP_VIDEOPROFILE_H264SCALABLEHIGH;
    case media::H264PROFILE_STEREOHIGH:
      return PP_VIDEOPROFILE_H264STEREOHIGH;
    case media::H264PROFILE_MULTIVIEWHIGH:
      return PP_VIDEOPROFILE_H264MULTIVIEWHIGH;
    case media::VP8PROFILE_ANY:
      return PP_VIDEOPROFILE_VP8_ANY;
    case media::VP9PROFILE_ANY:
      return PP_VIDEOPROFILE_VP9_ANY;
    default:
      NOTREACHED();
      return static_cast<PP_VideoProfile>(-1);
  }
}

media::VideoFrame::Format PP_ToMediaVideoFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_UNKNOWN:
      return media::VideoFrame::UNKNOWN;
    case PP_VIDEOFRAME_FORMAT_YV12:
      return media::VideoFrame::YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return media::VideoFrame::I420;
    case PP_VIDEOFRAME_FORMAT_BGRA:
      return media::VideoFrame::UNKNOWN;
    // No default case, to catch unhandled PP_VideoFrame_Format values.
  }
  return media::VideoFrame::UNKNOWN;
}

PP_VideoFrame_Format PP_FromMediaVideoFormat(media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::UNKNOWN:
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
    case media::VideoFrame::YV12:
      return PP_VIDEOFRAME_FORMAT_YV12;
    case media::VideoFrame::I420:
      return PP_VIDEOFRAME_FORMAT_I420;
    default:
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
}

PP_VideoProfileDescription PP_FromVideoEncodeAcceleratorSupportedProfile(
    media::VideoEncodeAccelerator::SupportedProfile profile,
    PP_HardwareAcceleration acceleration) {
  PP_VideoProfileDescription pp_profile;
  pp_profile.profile = PP_FromMediaVideoProfile(profile.profile);
  pp_profile.max_resolution = PP_FromGfxSize(profile.max_resolution);
  pp_profile.max_framerate_numerator = profile.max_framerate_numerator;
  pp_profile.max_framerate_denominator = profile.max_framerate_denominator;
  pp_profile.acceleration = acceleration;
  return pp_profile;
}

bool PP_HardwareAccelerationCompatible(PP_HardwareAcceleration supply,
                                       PP_HardwareAcceleration demand) {
  // TODO(llandwerlin): Change API to use bool instead of
  // PP_HardwareAcceleration
  switch (supply) {
    case PP_HARDWAREACCELERATION_ONLY:
      return (demand == PP_HARDWAREACCELERATION_ONLY ||
              demand == PP_HARDWAREACCELERATION_WITHFALLBACK);
    case PP_HARDWAREACCELERATION_WITHFALLBACK:
      return true;
    case PP_HARDWAREACCELERATION_NONE:
      return (demand == PP_HARDWAREACCELERATION_WITHFALLBACK ||
              demand == PP_HARDWAREACCELERATION_NONE);
    // No default case, to catch unhandled PP_HardwareAcceleration values.
  }
  return false;
}

}  // namespace

PepperVideoEncoderHost::ShmBuffer::ShmBuffer(uint32_t id,
                                             scoped_ptr<base::SharedMemory> shm)
    : id(id), shm(shm.Pass()), in_use(true) {
  DCHECK(this->shm);
}

PepperVideoEncoderHost::ShmBuffer::~ShmBuffer() {
}

media::BitstreamBuffer PepperVideoEncoderHost::ShmBuffer::ToBitstreamBuffer() {
  return media::BitstreamBuffer(id, shm->handle(), shm->mapped_size());
}

PepperVideoEncoderHost::PepperVideoEncoderHost(RendererPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      buffer_manager_(this),
      command_buffer_(nullptr),
      initialized_(false),
      encoder_last_error_(PP_ERROR_FAILED),
      frame_count_(0),
      media_input_format_(media::VideoFrame::UNKNOWN),
      weak_ptr_factory_(this) {
}

PepperVideoEncoderHost::~PepperVideoEncoderHost() {
  Close();
}

int32_t PepperVideoEncoderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperVideoEncoderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoEncoder_GetSupportedProfiles,
        OnHostMsgGetSupportedProfiles)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoEncoder_Initialize,
                                      OnHostMsgInitialize)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoEncoder_GetVideoFrames,
        OnHostMsgGetVideoFrames)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoEncoder_Encode,
                                      OnHostMsgEncode)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoEncoder_RecycleBitstreamBuffer,
        OnHostMsgRecycleBitstreamBuffer)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoEncoder_RequestEncodingParametersChange,
        OnHostMsgRequestEncodingParametersChange)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoEncoder_Close,
                                        OnHostMsgClose)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperVideoEncoderHost::OnHostMsgGetSupportedProfiles(
    ppapi::host::HostMessageContext* context) {
  std::vector<PP_VideoProfileDescription> pp_profiles;
  GetSupportedProfiles(&pp_profiles);

  host()->SendReply(
      context->MakeReplyMessageContext(),
      PpapiPluginMsg_VideoEncoder_GetSupportedProfilesReply(pp_profiles));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoEncoderHost::OnHostMsgInitialize(
    ppapi::host::HostMessageContext* context,
    PP_VideoFrame_Format input_format,
    const PP_Size& input_visible_size,
    PP_VideoProfile output_profile,
    uint32_t initial_bitrate,
    PP_HardwareAcceleration acceleration) {
  if (initialized_)
    return PP_ERROR_FAILED;

  media_input_format_ = PP_ToMediaVideoFormat(input_format);
  if (media_input_format_ == media::VideoFrame::UNKNOWN)
    return PP_ERROR_BADARGUMENT;

  media::VideoCodecProfile media_profile =
      PP_ToMediaVideoProfile(output_profile);
  if (media_profile == media::VIDEO_CODEC_PROFILE_UNKNOWN)
    return PP_ERROR_BADARGUMENT;

  gfx::Size input_size(input_visible_size.width, input_visible_size.height);
  if (input_size.IsEmpty())
    return PP_ERROR_BADARGUMENT;

  if (!IsInitializationValid(input_visible_size, output_profile, acceleration))
    return PP_ERROR_NOTSUPPORTED;

  int32_t error = PP_ERROR_NOTSUPPORTED;
  initialize_reply_context_ = context->MakeReplyMessageContext();

  if (acceleration != PP_HARDWAREACCELERATION_NONE) {
    if (InitializeHardware(media_input_format_, input_size, media_profile,
                           initial_bitrate))
      return PP_OK_COMPLETIONPENDING;

    if (acceleration == PP_HARDWAREACCELERATION_ONLY)
      error = PP_ERROR_FAILED;
  }

#if defined(OS_ANDROID)
  initialize_reply_context_ = ppapi::host::ReplyMessageContext();
  Close();
  return error;
#else
  if (acceleration != PP_HARDWAREACCELERATION_ONLY) {
    encoder_.reset(new VideoEncoderShim(this));
    if (encoder_->Initialize(media_input_format_, input_size, media_profile,
                             initial_bitrate, this))
      return PP_OK_COMPLETIONPENDING;
    error = PP_ERROR_FAILED;
  }

  initialize_reply_context_ = ppapi::host::ReplyMessageContext();
  Close();
  return error;
#endif
}

int32_t PepperVideoEncoderHost::OnHostMsgGetVideoFrames(
    ppapi::host::HostMessageContext* context) {
  if (encoder_last_error_)
    return encoder_last_error_;

  get_video_frames_reply_context_ = context->MakeReplyMessageContext();
  AllocateVideoFrames();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoEncoderHost::OnHostMsgEncode(
    ppapi::host::HostMessageContext* context,
    uint32_t frame_id,
    bool force_keyframe) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (frame_id >= frame_count_)
    return PP_ERROR_FAILED;

  encoder_->Encode(
      CreateVideoFrame(frame_id, context->MakeReplyMessageContext()),
      force_keyframe);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoEncoderHost::OnHostMsgRecycleBitstreamBuffer(
    ppapi::host::HostMessageContext* context,
    uint32_t buffer_id) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (buffer_id >= shm_buffers_.size() || shm_buffers_[buffer_id]->in_use)
    return PP_ERROR_FAILED;

  shm_buffers_[buffer_id]->in_use = true;
  encoder_->UseOutputBitstreamBuffer(
      shm_buffers_[buffer_id]->ToBitstreamBuffer());

  return PP_OK;
}

int32_t PepperVideoEncoderHost::OnHostMsgRequestEncodingParametersChange(
    ppapi::host::HostMessageContext* context,
    uint32_t bitrate,
    uint32_t framerate) {
  if (encoder_last_error_)
    return encoder_last_error_;

  encoder_->RequestEncodingParametersChange(bitrate, framerate);

  return PP_OK;
}

int32_t PepperVideoEncoderHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context) {
  encoder_last_error_ = PP_ERROR_FAILED;
  Close();

  return PP_OK;
}

void PepperVideoEncoderHost::RequireBitstreamBuffers(
    unsigned int frame_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK(RenderThreadImpl::current());
  // We assume RequireBitstreamBuffers is only called once.
  DCHECK(!initialized_);

  input_coded_size_ = input_coded_size;
  frame_count_ = frame_count;

  for (uint32_t i = 0; i < kDefaultNumberOfBitstreamBuffers; ++i) {
    scoped_ptr<base::SharedMemory> shm(
        RenderThread::Get()
            ->HostAllocateSharedMemoryBuffer(output_buffer_size)
            .Pass());

    if (!shm || !shm->Map(output_buffer_size)) {
      shm_buffers_.clear();
      break;
    }

    shm_buffers_.push_back(new ShmBuffer(i, shm.Pass()));
  }

  // Feed buffers to the encoder.
  std::vector<SerializedHandle> handles;
  for (size_t i = 0; i < shm_buffers_.size(); ++i) {
    encoder_->UseOutputBitstreamBuffer(shm_buffers_[i]->ToBitstreamBuffer());
    handles.push_back(SerializedHandle(
        renderer_ppapi_host_->ShareHandleWithRemote(
            PlatformFileFromSharedMemoryHandle(shm_buffers_[i]->shm->handle()),
            false),
        output_buffer_size));
  }

  host()->SendUnsolicitedReplyWithHandles(
      pp_resource(), PpapiPluginMsg_VideoEncoder_BitstreamBuffers(
                         static_cast<uint32_t>(output_buffer_size)),
      handles);

  if (!initialized_) {
    // Tell the plugin that initialization has been successful if we
    // haven't already.
    initialized_ = true;
    encoder_last_error_ = PP_OK;
    host()->SendReply(initialize_reply_context_,
                      PpapiPluginMsg_VideoEncoder_InitializeReply(
                          frame_count, PP_FromGfxSize(input_coded_size)));
  }

  if (shm_buffers_.empty()) {
    NotifyPepperError(PP_ERROR_NOMEMORY);
    return;
  }

  // If the plugin already requested video frames, we can now answer
  // that request.
  if (get_video_frames_reply_context_.is_valid())
    AllocateVideoFrames();
}

void PepperVideoEncoderHost::BitstreamBufferReady(int32 buffer_id,
                                                  size_t payload_size,
                                                  bool key_frame) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(shm_buffers_[buffer_id]->in_use);

  shm_buffers_[buffer_id]->in_use = false;
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoEncoder_BitstreamBufferReady(
          buffer_id, static_cast<uint32_t>(payload_size), key_frame));
}

void PepperVideoEncoderHost::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK(RenderThreadImpl::current());
  NotifyPepperError(PP_FromMediaEncodeAcceleratorError(error));
}

void PepperVideoEncoderHost::GetSupportedProfiles(
    std::vector<PP_VideoProfileDescription>* pp_profiles) {
  DCHECK(RenderThreadImpl::current());

  media::VideoEncodeAccelerator::SupportedProfiles profiles;

  if (EnsureGpuChannel()) {
    profiles = GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
        channel_->gpu_info().video_encode_accelerator_supported_profiles);
    for (media::VideoEncodeAccelerator::SupportedProfile profile : profiles) {
      pp_profiles->push_back(PP_FromVideoEncodeAcceleratorSupportedProfile(
          profile, PP_HARDWAREACCELERATION_ONLY));
    }
  }

#if !defined(OS_ANDROID)
  VideoEncoderShim software_encoder(this);
  profiles = software_encoder.GetSupportedProfiles();
  for (media::VideoEncodeAccelerator::SupportedProfile profile : profiles) {
    pp_profiles->push_back(PP_FromVideoEncodeAcceleratorSupportedProfile(
        profile, PP_HARDWAREACCELERATION_NONE));
  }
#endif
}

bool PepperVideoEncoderHost::IsInitializationValid(
    const PP_Size& input_size,
    PP_VideoProfile output_profile,
    PP_HardwareAcceleration acceleration) {
  DCHECK(RenderThreadImpl::current());

  std::vector<PP_VideoProfileDescription> profiles;
  GetSupportedProfiles(&profiles);

  for (const PP_VideoProfileDescription& profile : profiles) {
    if (output_profile == profile.profile &&
        input_size.width <= profile.max_resolution.width &&
        input_size.height <= profile.max_resolution.height &&
        PP_HardwareAccelerationCompatible(profile.acceleration, acceleration))
      return true;
  }

  return false;
}

bool PepperVideoEncoderHost::EnsureGpuChannel() {
  DCHECK(RenderThreadImpl::current());

  if (command_buffer_)
    return true;

  // There is no guarantee that we have a 3D context to work with. So
  // we create a dummy command buffer to communicate with the gpu process.
  channel_ = RenderThreadImpl::current()->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_PEPPERVIDEOENCODERACCELERATOR_INITIALIZE);
  if (!channel_)
    return false;

  std::vector<int32> attribs(1, PP_GRAPHICS3DATTRIB_NONE);
  command_buffer_ = channel_->CreateOffscreenCommandBuffer(
      gfx::Size(), nullptr, attribs, GURL::EmptyGURL(),
      gfx::PreferIntegratedGpu);
  if (!command_buffer_) {
    Close();
    return false;
  }

  command_buffer_->SetChannelErrorCallback(media::BindToCurrentLoop(
      base::Bind(&PepperVideoEncoderHost::NotifyPepperError,
                 weak_ptr_factory_.GetWeakPtr(), PP_ERROR_RESOURCE_FAILED)));
  if (!command_buffer_->Initialize()) {
    Close();
    return false;
  }

  return true;
}

bool PepperVideoEncoderHost::InitializeHardware(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32_t initial_bitrate) {
  DCHECK(RenderThreadImpl::current());

  if (!EnsureGpuChannel())
    return false;

  encoder_ = command_buffer_->CreateVideoEncoder();
  if (!encoder_ ||
      !encoder_->Initialize(input_format, input_visible_size, output_profile,
                            initial_bitrate, this))
    return false;

  return true;
}

void PepperVideoEncoderHost::Close() {
  DCHECK(RenderThreadImpl::current());

  encoder_ = nullptr;
  if (command_buffer_) {
    DCHECK(channel_);
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = nullptr;
  }
}

void PepperVideoEncoderHost::AllocateVideoFrames() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(get_video_frames_reply_context_.is_valid());

  // Frames have already been allocated.
  if (buffer_manager_.number_of_buffers() > 0) {
    SendGetFramesErrorReply(PP_ERROR_FAILED);
    NOTREACHED();
    return;
  }

  base::CheckedNumeric<uint32_t> size =
      media::VideoFrame::AllocationSize(media_input_format_, input_coded_size_);
  uint32_t frame_size = size.ValueOrDie();
  size += sizeof(ppapi::MediaStreamBuffer::Video);
  uint32_t buffer_size = size.ValueOrDie();
  // Make each buffer 4 byte aligned.
  size += (4 - buffer_size % 4);
  uint32_t buffer_size_aligned = size.ValueOrDie();
  size *= frame_count_;
  uint32_t total_size = size.ValueOrDie();

  scoped_ptr<base::SharedMemory> shm(
      RenderThreadImpl::current()
          ->HostAllocateSharedMemoryBuffer(total_size)
          .Pass());
  if (!shm ||
      !buffer_manager_.SetBuffers(frame_count_,
                                  buffer_size_aligned,
                                  shm.Pass(),
                                  true)) {
    SendGetFramesErrorReply(PP_ERROR_NOMEMORY);
    return;
  }

  VLOG(4) << " frame_count=" << frame_count_ << " frame_size=" << frame_size
          << " buffer_size=" << buffer_size_aligned;

  for (int32_t i = 0; i < buffer_manager_.number_of_buffers(); ++i) {
    ppapi::MediaStreamBuffer::Video* buffer =
        &(buffer_manager_.GetBufferPointer(i)->video);
    buffer->header.size = buffer_manager_.buffer_size();
    buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
    buffer->format = PP_FromMediaVideoFormat(media_input_format_);
    buffer->size.width = input_coded_size_.width();
    buffer->size.height = input_coded_size_.height();
    buffer->data_size = frame_size;
  }

  DCHECK(get_video_frames_reply_context_.is_valid());
  get_video_frames_reply_context_.params.AppendHandle(SerializedHandle(
      renderer_ppapi_host_->ShareHandleWithRemote(
          PlatformFileFromSharedMemoryHandle(buffer_manager_.shm()->handle()),
          false),
      total_size));

  host()->SendReply(get_video_frames_reply_context_,
                    PpapiPluginMsg_VideoEncoder_GetVideoFramesReply(
                        frame_count_, buffer_size_aligned,
                        PP_FromGfxSize(input_coded_size_)));
  get_video_frames_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoEncoderHost::SendGetFramesErrorReply(int32_t error) {
  get_video_frames_reply_context_.params.set_result(error);
  host()->SendReply(
      get_video_frames_reply_context_,
      PpapiPluginMsg_VideoEncoder_GetVideoFramesReply(0, 0, PP_MakeSize(0, 0)));
  get_video_frames_reply_context_ = ppapi::host::ReplyMessageContext();
}

scoped_refptr<media::VideoFrame> PepperVideoEncoderHost::CreateVideoFrame(
    uint32_t frame_id,
    const ppapi::host::ReplyMessageContext& reply_context) {
  DCHECK(RenderThreadImpl::current());

  ppapi::MediaStreamBuffer* buffer = buffer_manager_.GetBufferPointer(frame_id);
  DCHECK(buffer);
  uint32_t shm_offset = static_cast<uint8*>(buffer->video.data) -
                        static_cast<uint8*>(buffer_manager_.shm()->memory());

  return media::VideoFrame::WrapExternalPackedMemory(
      media_input_format_, input_coded_size_, gfx::Rect(input_coded_size_),
      input_coded_size_, static_cast<uint8*>(buffer->video.data),
      buffer->video.data_size, buffer_manager_.shm()->handle(), shm_offset,
      base::TimeDelta(),
      base::Bind(&PepperVideoEncoderHost::FrameReleased,
                 weak_ptr_factory_.GetWeakPtr(), reply_context, frame_id));
}

void PepperVideoEncoderHost::FrameReleased(
    const ppapi::host::ReplyMessageContext& reply_context,
    uint32_t frame_id) {
  DCHECK(RenderThreadImpl::current());

  ppapi::host::ReplyMessageContext context = reply_context;
  context.params.set_result(encoder_last_error_);
  host()->SendReply(context, PpapiPluginMsg_VideoEncoder_EncodeReply(frame_id));
}

void PepperVideoEncoderHost::NotifyPepperError(int32_t error) {
  DCHECK(RenderThreadImpl::current());

  encoder_last_error_ = error;
  Close();
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoEncoder_NotifyError(encoder_last_error_));
}

uint8_t* PepperVideoEncoderHost::ShmHandleToAddress(int32 buffer_id) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_GE(buffer_id, 0);
  DCHECK_LT(buffer_id, static_cast<int32>(shm_buffers_.size()));
  return static_cast<uint8_t*>(shm_buffers_[buffer_id]->shm->memory());
}

}  // namespace content
