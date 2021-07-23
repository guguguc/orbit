// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_BASIC_PAGE_FAULTS_TRACK_H_
#define ORBIT_GL_BASIC_PAGE_FAULTS_TRACK_H_

#include <string>
#include <utility>

#include "AnnotationTrack.h"
#include "LineGraphTrack.h"
#include "Track.h"
#include "Viewport.h"

namespace orbit_gl {

constexpr size_t kBasicPageFaultsTrackDimension = 3;

// This is a implementation of `LineGraphTrack` to display major or minor page faults information,
// used in the `PageFaultsTrack`.
class BasicPageFaultsTrack : public LineGraphTrack<kBasicPageFaultsTrackDimension>,
                             public AnnotationTrack {
 public:
  explicit BasicPageFaultsTrack(Track* parent, TimeGraph* time_graph, orbit_gl::Viewport* viewport,
                                TimeGraphLayout* layout, const std::string& name,
                                const std::string& cgroup_name, uint64_t memory_sampling_period_ms,
                                const orbit_client_model::CaptureData* capture_data);

  [[nodiscard]] Track* GetParent() const override { return parent_; }
  // For subtracks there is no meaningful type and it should also not be exposed, though we use the
  // unknown type.
  [[nodiscard]] Track::Type GetType() const override { return Track::Type::kUnknown; }

  void AddValues(uint64_t timestamp_ns,
                 const std::array<double, kBasicPageFaultsTrackDimension>& values) override;
  void AddValuesAndUpdateAnnotations(
      uint64_t timestamp_ns, const std::array<double, kBasicPageFaultsTrackDimension>& values);

  void Draw(Batcher& batcher, TextRenderer& text_renderer, uint64_t current_mouse_time_ns,
            PickingMode picking_mode, uint32_t indentation_level, float z_offset = 0) override;

  enum class SeriesIndex { kProcess = 0, kCGroup = 1, kSystem = 2 };

 protected:
  void DrawSingleSeriesEntry(
      Batcher* batcher, uint64_t start_tick, uint64_t end_tick,
      const std::array<float, kBasicPageFaultsTrackDimension>& current_normalized_values,
      const std::array<float, kBasicPageFaultsTrackDimension>& next_normalized_values, float z,
      bool is_last) override;

  // Once this is set, if values[index_of_series_to_highlight_] > 0 in the sampling window t, we
  // will draw a colored box in this sampling window to highlight the occurrence of page faults
  // series_name[index_of_series_to_highlight_].
  std::optional<size_t> index_of_series_to_highlight_ = std::nullopt;
  std::string cgroup_name_;
  uint64_t memory_sampling_period_ms_;

 private:
  [[nodiscard]] bool IsCollapsed() const override;
  [[nodiscard]] float GetAnnotatedTrackContentHeight() const override;
  [[nodiscard]] Vec2 GetAnnotatedTrackPosition() const override { return pos_; };
  [[nodiscard]] Vec2 GetAnnotatedTrackSize() const override { return size_; };
  [[nodiscard]] uint32_t GetAnnotationFontSize(int indentation_level) const override {
    return GetLegendFontSize(indentation_level);
  }

  Track* parent_;
  std::optional<std::pair<uint64_t, std::array<double, kBasicPageFaultsTrackDimension>>>
      previous_time_and_values_ = std::nullopt;
};

}  // namespace orbit_gl

#endif  // ORBIT_GL_BASIC_PAGE_FAULTS_TRACK_H_
