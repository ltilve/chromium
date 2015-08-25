// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/webm_muxer.h"

#include <limits>

#include "base/bind.h"
#include "media/base/video_frame.h"

namespace media {

static double GetFrameRate(const scoped_refptr<VideoFrame>& video_frame) {
  double frame_rate = 0.0f;
  base::IgnoreResult(video_frame->metadata()->GetDouble(
      VideoFrameMetadata::FRAME_RATE, &frame_rate));
  return frame_rate;
}

WebmMuxer::WebmMuxer(const WriteDataCB& write_data_callback)
    : track_index_(0),
      write_data_callback_(write_data_callback),
      position_(0) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!write_data_callback_.is_null());
  segment_.Init(this);
  segment_.set_mode(mkvmuxer::Segment::kLive);
  segment_.OutputCues(false);

  mkvmuxer::SegmentInfo* const info = segment_.GetSegmentInfo();
  info->set_writing_app("Chrome");
  info->set_muxing_app("Chrome");
}

WebmMuxer::~WebmMuxer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  segment_.Finalize();
}

void WebmMuxer::OnEncodedVideo(const scoped_refptr<VideoFrame>& video_frame,
                               const base::StringPiece& encoded_data,
                               base::TimeTicks timestamp,
                               bool is_key_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!track_index_) {
    // |track_index_|, cannot be zero (!), initialize WebmMuxer in that case.
    // http://www.matroska.org/technical/specs/index.html#Tracks
    AddVideoTrack(video_frame->visible_rect().size(),
                  GetFrameRate(video_frame));
    first_frame_timestamp_ = timestamp;
  }
  segment_.AddFrame(reinterpret_cast<const uint8_t*>(encoded_data.data()),
                    encoded_data.size(),
                    track_index_,
                    (timestamp - first_frame_timestamp_).InMilliseconds(),
                    is_key_frame);
}

void WebmMuxer::AddVideoTrack(const gfx::Size& frame_size, double frame_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(track_index_, 0u);
  track_index_ =
      segment_.AddVideoTrack(frame_size.width(), frame_size.height(), 0);
  DCHECK_GT(track_index_, 0u);

  mkvmuxer::VideoTrack* const video_track =
      reinterpret_cast<mkvmuxer::VideoTrack*>(
          segment_.GetTrackByNumber(track_index_));
  DCHECK(video_track);
  video_track->set_codec_id(mkvmuxer::Tracks::kVp8CodecId);
  DCHECK_EQ(video_track->crop_right(), 0ull);
  DCHECK_EQ(video_track->crop_left(), 0ull);
  DCHECK_EQ(video_track->crop_top(), 0ull);
  DCHECK_EQ(video_track->crop_bottom(), 0ull);

  video_track->set_frame_rate(frame_rate);
  video_track->set_default_duration(base::Time::kMicrosecondsPerSecond /
                                    frame_rate);
  // Segment's timestamps should be in milliseconds, DCHECK it. See
  // http://www.webmproject.org/docs/container/#muxer-guidelines
  DCHECK_EQ(segment_.GetSegmentInfo()->timecode_scale(), 1000000ull);
}

mkvmuxer::int32 WebmMuxer::Write(const void* buf, mkvmuxer::uint32 len) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buf);
  write_data_callback_.Run(base::StringPiece(reinterpret_cast<const char*>(buf),
                                             len));
  position_ += len;
  return 0;
}

mkvmuxer::int64 WebmMuxer::Position() const {
  return position_.ValueOrDie();
}

mkvmuxer::int32 WebmMuxer::Position(mkvmuxer::int64 position) {
  // The stream is not Seekable() so indicate we cannot set the position.
  return -1;
}

bool WebmMuxer::Seekable() const {
  return false;
}

void WebmMuxer::ElementStartNotify(mkvmuxer::uint64 element_id,
                                   mkvmuxer::int64 position) {
  // This method gets pinged before items are sent to |write_data_callback_|.
  DCHECK_GE(position, position_.ValueOrDefault(0))
      << "Can't go back in a live WebM stream.";
}

}  // namespace media
