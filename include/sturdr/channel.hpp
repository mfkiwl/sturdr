/**
 * *channel.hpp*
 *
 * =======  ========================================================================================
 * @file    sturdr/channel.hpp
 * @brief   Abstract class for GNSS channel definitions.
 * @date    January 2025
 * @ref     1. "Understanding GPS/GNSS Principles and Applications", 3rd Edition, 2017
 *             - Kaplan & Hegarty
 *          2. "A Software-Defined GPS and Galileo Receiver: A Single-Frequency Approach", 2007
 *             - Borre, Akos, Bertelsen, Rinder, Jensen
 * =======  ========================================================================================
 */

#ifndef STURDR_CHANNEL_HPP
#define STURDR_CHANNEL_HPP

// #include <spdlog/async.h>
// #include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <Eigen/Dense>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "sturdr/concurrent-barrier.hpp"
#include "sturdr/concurrent-queue.hpp"
#include "sturdr/fftw-wrapper.hpp"
#include "sturdr/structs-enums.hpp"

namespace sturdr {

class Channel {
 protected:
  /**
   * @brief channel config
   */
  Config conf_;
  std::shared_ptr<bool> running_;
  uint64_t samp_per_ms_;
  uint8_t acq_fail_cnt_;
  std::shared_ptr<FftwWrapper> fftw_plans_;
  std::function<void(uint8_t &)> new_prn_func_;

  /**
   * @brief thread syncronization
   */
  std::shared_ptr<Eigen::MatrixXcd> shm_;
  uint64_t shm_ptr_;
  uint64_t shm_writer_ptr_;
  uint64_t shm_file_size_samp_;
  uint64_t shm_read_size_samp_;
  std::shared_ptr<ConcurrentBarrier> barrier1_;
  std::shared_ptr<ConcurrentBarrier> barrier2_;
  std::shared_ptr<ConcurrentQueue> q_nav_;
  std::shared_ptr<std::thread> thread_;
  ChannelEphemPacket eph_pkt_;
  ChannelNavPacket nav_pkt_;

  /**
   * @brief spdlog loggers
   */
  ChannelPacket file_pkt_;
  std::shared_ptr<spdlog::logger> log_;
  // std::shared_ptr<spdlog::logger> file_log_;
  std::shared_ptr<std::ofstream> file_log_;

 public:
  /**
   * @brief Constructor
   * @param conf          SturDR yaml configuration
   * @param n             Channel ID number
   * @param running       Boolean for SturDR active state
   * @param start_barrier Synchronization for when new data is available
   * @param eph_queue     Queue for sending parsed ephemerides
   * @param nav_queue     Queue for sending navigation updates
   * @param fftw_plans    Shared FFT plans for acquisition using fftw
   * @param GetNewPrnFunc Function pointer for channel capability to switch PRNs
   */
  Channel(
      Config &conf,
      uint8_t &n,
      std::shared_ptr<bool> running,
      std::shared_ptr<Eigen::MatrixXcd> shared_array,
      std::shared_ptr<ConcurrentBarrier> barrier1,
      std::shared_ptr<ConcurrentBarrier> barrier2,
      std::shared_ptr<ConcurrentQueue> nav_queue,
      std::shared_ptr<FftwWrapper> fftw_plans,
      std::function<void(uint8_t &)> &GetNewPrnFunc)
      : conf_{conf},
        running_{running},
        samp_per_ms_{static_cast<uint64_t>(conf_.rfsignal.samp_freq) / 1000},
        acq_fail_cnt_{0},
        fftw_plans_{fftw_plans},
        new_prn_func_{GetNewPrnFunc},
        shm_{shared_array},
        shm_ptr_{0},
        shm_writer_ptr_{0},
        shm_file_size_samp_{conf_.general.ms_chunk_size * samp_per_ms_},
        shm_read_size_samp_{conf_.general.ms_read_size * samp_per_ms_},
        barrier1_{barrier1},
        barrier2_{barrier2},
        q_nav_{nav_queue},
        // thread_{std::make_shared<std::thread>(&Channel::Run, this)},
        eph_pkt_{ChannelEphemPacket()},
        nav_pkt_{ChannelNavPacket()},
        file_pkt_{ChannelPacket()},
        log_{spdlog::get("sturdr-console")},
        file_log_{std::make_shared<std::ofstream>(
            conf.general.out_folder + "/" + conf.general.scenario + "/SturDR_Ch" +
                std::to_string(n) + "_Log.bin",
            std::ios::binary | std::ios::trunc)}
  // file_log_{spdlog::basic_logger_st<spdlog::async_factory>(
  //     "sturdr-ch" + std::to_string(n) + "-log",
  //     conf.general.out_folder + "/" + conf.general.scenario + "/SturDR_Ch" +
  //         std::to_string(n) + "_Log.csv",
  //     true)}
  {
    file_pkt_.ChannelStatus = ChannelState::ACQUIRING;
    file_pkt_.Header.ChannelNum = n;
    eph_pkt_.Header.ChannelNum = n;
    nav_pkt_.Header.ChannelNum = n;
    // file_log_->set_pattern("%v");
    // file_log_->info(
    //     "ChannelNum,Constellation,Signal,SVID,ChannelStatus,TrackingStatus,Week,ToW,"
    //     "CNo,Doppler,CodePhase,CarrierPhase,IE,IP,IL,QE,QP,QL,IP1,IP2,QP1,QP2,"
    //     "DllDisc,PllDisc,FllDisc,IP_A0,IP_A1,IP_A2,IP_A3,QP_A0,QP_A1,QP_A2,QP_A3");
  };

  /**
   * *=== ~Channel ===*
   * @brief Destructor
   */
  virtual ~Channel() {
    file_log_->close();
    if (thread_->joinable()) {
      thread_->join();
    }
  };

  void Start() {
    thread_ = std::make_shared<std::thread>(&Channel::Run, this);
  }

  void Join() {
    if (thread_->joinable()) {
      thread_->join();
    }
  }

  /**
   * *=== Run ===*
   * @brief Main channel thread
   */
  void Run() {
    // wait for initial shm data to be added
    barrier1_->Wait();
    UpdateShmWriterPtr();

    // continue processing data until SturDR ends
    while (*running_) {
      // process
      barrier2_->Wait();
      switch (file_pkt_.ChannelStatus) {
        case ChannelState::IDLE:
          break;
        case ChannelState::ACQUIRING:
          Acquire();
          break;
        case ChannelState::TRACKING:
          Track();
          break;
      }

      // wait for new shm data
      barrier1_->Wait();
      UpdateShmWriterPtr();
    }
    log_->debug("Channel {} stopping ...", file_pkt_.Header.ChannelNum);
  };

 protected:
  /**
   * *=== Acquire ===*
   * @brief Trys to acquire current satellite
   */
  virtual void Acquire() = 0;

  /**
   * *=== Track ===*
   * @brief Trys to track current satellite
   */
  virtual void Track() = 0;
  virtual void Integrate(const uint64_t &samp_to_read) = 0;
  virtual void Dump() = 0;

  /**
   * *=== NavDataSync ===*
   * @brief Trys to synchronize to the data bit and extend the integration periods
   */
  virtual bool NavDataSync() = 0;

  /**
   * *=== Demodulate ===*
   * @brief Trys to demodulate navigation data and parse ephemerides
   */
  virtual void Demodulate() = 0;

  /**
   * *=== UpdateShmWriterPtr ===*
   * @brief Keeps track of where the shm_ writer is so channel does not read more than is written
   */
  void UpdateShmWriterPtr() {
    shm_writer_ptr_ += shm_read_size_samp_;
    shm_writer_ptr_ %= shm_file_size_samp_;
  }

  /**
   * *=== UnreadSampleCount ===*
   * @brief Returns the difference between shm_write_ptr_ and shm_
   */
  uint64_t UnreadSampleCount() {
    if (shm_ptr_ <= shm_writer_ptr_) {
      return shm_writer_ptr_ - shm_ptr_;
    } else {
      return shm_file_size_samp_ - shm_ptr_ + shm_writer_ptr_;
    }
  }
};

}  // namespace sturdr

#endif