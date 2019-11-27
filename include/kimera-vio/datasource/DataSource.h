/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   DataSource.h
 * @brief  Base implementation of a data provider for the VIO pipeline.
 * @author Antoni Rosinol
 */

#pragma once

#include <functional>
#include <string>

#include "kimera-vio/frontend/Camera.h"
#include "kimera-vio/frontend/StereoImuSyncPacket.h"
#include "kimera-vio/pipeline/Pipeline-definitions.h"
#include "kimera-vio/pipeline/PipelineModule.h"
#include "kimera-vio/utils/Macros.h"

namespace VIO {
class DataProviderInterface {
 public:
  KIMERA_DELETE_COPY_CONSTRUCTORS(DataProviderInterface);
  KIMERA_POINTER_TYPEDEFS(DataProviderInterface);
  typedef std::function<void(StereoImuSyncPacket::UniquePtr)> VioInputCallback;

  /** Regular ctor.
   *   [in] initial_k: first frame id to be parsed.
   *   [in] final_k: last frame id to be parsed.
   *   [in] dataset_path: path to the Euroc dataset.
   **/
  DataProviderInterface(int initial_k,
                        int final_k,
                        const std::string& dataset_path);
  // Ctor from gflags. Calls regular ctor with gflags values.
  DataProviderInterface();
  virtual ~DataProviderInterface();

  // The derived classes need to implement this function!
  // Spin the dataset: processes the input data and constructs a Stereo Imu
  // Synchronized Packet which contains the minimum amount of information
  // for the VIO pipeline to do one processing iteration.
  // A Dummy example is provided as an implementation.
  virtual bool spin();

  // Init Vio parameters.
  PipelineParams pipeline_params_;

  // Register a callback function that will be called once a StereoImu Synchro-
  // nized packet is available for processing.
  void registerVioCallback(VioInputCallback callback);

 protected:
  // Vio callback. This function should be called once a StereoImuSyncPacket
  // is available for processing.
  VioInputCallback vio_callback_;

  FrameId initial_k_;  // start frame
  FrameId final_k_;    // end frame
  std::string dataset_path_;

protected:
 // TODO(Toni): Create a separate params only parser!
 //! Helper functions to parse user-specified parameters.
 //! These are agnostic to dataset type.
 void parseBackendParams();
 void parseFrontendParams();
 void parseLCDParams();

 //! Functions to parse dataset dependent parameters.
 // Parse cam0, cam1 of a given dataset.
 virtual bool parseCameraParams(const std::string& input_dataset_path,
                                const std::string& left_cam_name,
                                const std::string& right_cam_name,
                                const bool parse_images,
                                MultiCameraParams* multi_cam_params) = 0;
 virtual bool parseImuParams(const std::string& input_dataset_path,
                             const std::string& imu_name,
                             ImuParams* imu_params) = 0;
};

class DataProviderModule
    : public MISOPipelineModule<StereoImuSyncPacket, StereoImuSyncPacket> {
 public:
  KIMERA_DELETE_COPY_CONSTRUCTORS(DataProviderModule);
  KIMERA_POINTER_TYPEDEFS(DataProviderModule);
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  DataProviderModule(OutputQueue* output_queue,
                     const std::string& name_id,
                     const bool& parallel_run)
      : MISOPipelineModule<StereoImuSyncPacket, StereoImuSyncPacket>(
            output_queue,
            name_id,
            parallel_run),
        imu_data_(),
        left_frame_queue_("data_provider_left_frame_queue"),
        right_frame_queue_("data_provider_right_frame_queue") {}

  virtual ~DataProviderModule() = default;

  virtual OutputPtr spinOnce(StereoImuSyncPacket::UniquePtr input) override {
    // Data provider is only syncing input sensor information, which
    // is done at the level of getInputPacket, therefore here we h
    // TODO(TONI): WARNING we could be doing perfect forwarding here but we
    // aren't because spinOnce uses as parameter a const& instead of the unique
    // pointer itself.
    return std::move(input);
  }

  //! Callbacks to fill queues: they should be all lighting fast.
  inline void fillImuQueue(const ImuMeasurements& imu_measurements) {
    imu_data_.imu_buffer_.addMeasurements(imu_measurements.timestamps_,
                                          imu_measurements.measurements_);
  }
  inline void fillLeftFrameQueue(Frame::UniquePtr&& left_frame) {
    CHECK(left_frame);
    left_frame_queue_.push(std::move(left_frame));
  }
  inline void fillRightFrameQueue(Frame::UniquePtr&& right_frame) {
    CHECK(right_frame);
    right_frame_queue_.push(std::move(right_frame));
  }

 protected:
  virtual InputPtr getInputPacket() override {
    bool queue_state = false;
    Frame::UniquePtr left_frame_payload = nullptr;
    if (PIO::parallel_run_) {
      queue_state = left_frame_queue_.popBlocking(left_frame_payload);
    } else {
      queue_state = left_frame_queue_.pop(left_frame_payload);
    }

    if (!queue_state) {
      LOG_IF(WARNING, PIO::parallel_run_)
          << "Module: " << name_id_ << " - Mesher queue is down";
      VLOG_IF(1, !PIO::parallel_run_)
          << "Module: " << name_id_ << " - Mesher queue is empty or down";
      return nullptr;
    }

    CHECK(left_frame_payload);
    const Timestamp& timestamp = left_frame_payload->timestamp_;

    // Look for the synchronized packet in frontend payload queue
    // This should always work, because it should not be possible to have
    // a backend payload without having a frontend one first!
    Frame::UniquePtr right_frame_payload = nullptr;
    PIO::syncQueue(timestamp, &right_frame_queue_, &right_frame_payload);
    CHECK(right_frame_payload);

    static Timestamp timestamp_last_frame = 0;
    ImuMeasurements imu_meas;
    CHECK_LT(timestamp_last_frame, timestamp);
    CHECK(utils::ThreadsafeImuBuffer::QueryResult::kDataAvailable ==
          imu_data_.imu_buffer_.getImuDataInterpolatedUpperBorder(
              timestamp_last_frame,
              timestamp,
              &imu_meas.timestamps_,
              &imu_meas.measurements_));

    // Push the synced messages to the visualizer's input queue
    static FrameId k;
    k = k + 1;
    return VIO::make_unique<StereoImuSyncPacket>(
        StereoFrame(k,
                    timestamp,
                    *left_frame_payload,
                    *right_frame_payload,
                    StereoMatchingParams()),
        imu_meas.timestamps_,
        imu_meas.measurements_);
  }

 private:
  //! Input data
  ImuData imu_data_;
  ThreadsafeQueue<Frame::UniquePtr> left_frame_queue_;
  ThreadsafeQueue<Frame::UniquePtr> right_frame_queue_;
};

}  // namespace VIO
