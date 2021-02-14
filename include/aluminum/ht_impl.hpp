////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Lawrence Livermore National Security, LLC.  Produced at the
// Lawrence Livermore National Laboratory in collaboration with University of
// Illinois Urbana-Champaign.
//
// Written by the LBANN Research Team (N. Dryden, N. Maruyama, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-756777.
// All rights reserved.
//
// This file is part of Aluminum GPU-aware Communication Library. For details, see
// http://software.llnl.gov/Aluminum or https://github.com/LLNL/Aluminum.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Al.hpp"
#include "aluminum/ht/communicator.hpp"
#include "aluminum/ht/allgather.hpp"
#include "aluminum/ht/allreduce.hpp"
#include "aluminum/ht/alltoall.hpp"
#include "aluminum/ht/bcast.hpp"
#include "aluminum/ht/gather.hpp"
#include "aluminum/ht/reduce.hpp"
#include "aluminum/ht/reduce_scatter.hpp"
#include "aluminum/ht/scatter.hpp"
#include "aluminum/ht/pt2pt.hpp"

namespace Al {

enum class HostTransferAllreduceAlgorithm {
  automatic,
  host_transfer
};

enum class HostTransferCollectiveAlgorithm {
  automatic
};

inline std::string algorithm_name(HostTransferAllreduceAlgorithm algo) {
  switch (algo) {
  case HostTransferAllreduceAlgorithm::automatic:
    return "automatic";
  case HostTransferAllreduceAlgorithm::host_transfer:
    return "host-transfer";
  default:
    return "unknown";
  }
}

inline std::string algorithm_name(HostTransferCollectiveAlgorithm algo) {
  switch (algo) {
  case HostTransferCollectiveAlgorithm::automatic:
    return "automatic";
  default:
    return "unknown";
  }
}

namespace internal {
namespace ht {

/** Initialize host-transfer backend. */
void init(int& argc, char**& argv);
/** Finalize host-transfer backend. */
void finalize();

/** Represents a request for the host-transfer backend. */
struct HostTransferRequest {
  HostTransferRequest(cudaEvent_t op_event_, cudaStream_t orig_stream_) :
    op_event(op_event_), orig_stream(orig_stream_) {}
  // Note: Not thread safe!
  ~HostTransferRequest() { cuda::release_cuda_event(op_event); }
  /** Event pending on completion of the operation. */
  cudaEvent_t op_event;
  /** Original stream associated with the operation. */
  cudaStream_t orig_stream;
};

}  // namespace ht
}  // namespace internal

class HostTransferBackend {
  friend void internal::ht::init(int&, char**&);
  friend void internal::ht::finalize();
 public:
  using allreduce_algo_type = HostTransferAllreduceAlgorithm;
  using allgather_algo_type = HostTransferCollectiveAlgorithm;
  using alltoall_algo_type = HostTransferCollectiveAlgorithm;
  using bcast_algo_type = HostTransferCollectiveAlgorithm;
  using gather_algo_type = HostTransferCollectiveAlgorithm;
  using reduce_algo_type = HostTransferCollectiveAlgorithm;
  using reduce_scatter_algo_type = HostTransferCollectiveAlgorithm;
  using scatter_algo_type = HostTransferCollectiveAlgorithm;
  using comm_type = internal::ht::HostTransferCommunicator;
  using req_type = std::shared_ptr<internal::ht::HostTransferRequest>;
  static constexpr std::nullptr_t null_req = nullptr;

  template <typename T>
  static void Allreduce(const T* sendbuf, T* recvbuf, size_t count,
                        ReductionOperator op, comm_type& comm,
                        allreduce_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferAllreduceAlgorithm::automatic:
    case HostTransferAllreduceAlgorithm::host_transfer:
      do_allreduce(sendbuf, recvbuf, count, op, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("Invalid algorithm");
    }
  }
  template <typename T>
  static void Allreduce(T* recvbuf, size_t count,
                        ReductionOperator op, comm_type& comm,
                        allreduce_algo_type algo) {
    Allreduce(recvbuf, recvbuf, count, op, comm, algo);
  }

  template <typename T>
  static void NonblockingAllreduce(
      const T* sendbuf, T* recvbuf, size_t count,
      ReductionOperator op,
      comm_type& comm,
      req_type& req,
      allreduce_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferAllreduceAlgorithm::automatic:
    case HostTransferAllreduceAlgorithm::host_transfer:
      do_allreduce(sendbuf, recvbuf, count, op, comm, internal_stream);
      break;
    default:
      throw_al_exception("Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingAllreduce(
      T* recvbuf, size_t count,
      ReductionOperator op, comm_type& comm,
      req_type& req,
      allreduce_algo_type algo) {
    NonblockingAllreduce(recvbuf, recvbuf, count, op, comm,
                         req, algo);
  }

  template <typename T>
  static void Send(const T* sendbuf, size_t count, int dest, comm_type& comm) {
    do_send(sendbuf, count, dest, comm, comm.get_stream());
  }

  template <typename T>
  static void NonblockingSend(const T* sendbuf, size_t count, int dest,
                              comm_type& comm, req_type& req) {
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    do_send(sendbuf, count, dest, comm, internal_stream);
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void Recv(T* recvbuf, size_t count, int src, comm_type& comm) {
    do_recv(recvbuf, count, src, comm, comm.get_stream());
  }

  template <typename T>
  static void NonblockingRecv(T* recvbuf, size_t count, int src,
                              comm_type& comm, req_type& req) {
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    do_recv(recvbuf, count, src, comm, internal_stream);
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void SendRecv(const T* sendbuf, size_t send_count, int dest,
                       T* recvbuf, size_t recv_count, int src, comm_type& comm) {
    do_sendrecv(sendbuf, send_count, dest, recvbuf, recv_count, src, comm,
                comm.get_stream());
  }

  template <typename T>
  static void NonblockingSendRecv(const T* sendbuf, size_t send_count, int dest,
                                  T* recvbuf, size_t recv_count, int src,
                                  comm_type& comm, req_type& req) {
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    do_sendrecv(sendbuf, send_count, dest, recvbuf, recv_count, src, comm,
                internal_stream);
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void Allgather(
    const T* sendbuf, T* recvbuf, size_t count,
    comm_type& comm, allgather_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_allgather(sendbuf, recvbuf, count, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferAllgather: Invalid algorithm");
    }
  }

  template <typename T>
  static void Allgather(
    T* buffer, size_t count, comm_type& comm, allgather_algo_type algo) {
    Allgather(buffer, buffer, count, comm, algo);
  }

  template <typename T>
  static void NonblockingAllgather(
    const T* sendbuf, T* recvbuf, size_t count,
    comm_type& comm, req_type& req, allgather_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_allgather(sendbuf, recvbuf, count, comm, internal_stream);
      break;
    default:
      throw_al_exception("HostTransferAllgather: Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingAllgather(
    T* buffer, size_t count,
    comm_type& comm, req_type& req, allgather_algo_type algo) {
    NonblockingAllgather<T>(buffer, buffer, count, comm, req, algo);
  }

  template <typename T>
  static void Alltoall(
    const T* sendbuf, T* recvbuf, size_t count,
    comm_type& comm, alltoall_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_alltoall(sendbuf, recvbuf, count, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferAlltoall: Invalid algorithm");
    }
  }

  template <typename T>
  static void Alltoall(
    T* buffer, size_t count, comm_type& comm, alltoall_algo_type algo) {
    Alltoall(buffer, buffer, count, comm, algo);
  }

  template <typename T>
  static void NonblockingAlltoall(
    const T* sendbuf, T* recvbuf, size_t count,
    comm_type& comm, req_type& req, alltoall_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_alltoall(sendbuf, recvbuf, count, comm, internal_stream);
      break;
    default:
      throw_al_exception("Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingAlltoall(
    T* buffer, size_t count,
    comm_type& comm, req_type& req, alltoall_algo_type algo) {
    NonblockingAlltoall<T>(buffer, buffer, count, comm, req, algo);
  }

  template <typename T>
  static void Bcast(T* buf, size_t count, int root, comm_type& comm,
                    bcast_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_bcast(buf, count, root, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferBcast: Invalid algorithm");
    }
  }

  template <typename T>
  static void NonblockingBcast(
    T* buf, size_t count, int root,
    comm_type& comm, req_type& req, bcast_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_bcast(buf, count, root, comm, internal_stream);
      break;
    default:
      throw_al_exception("HostTransferBcast: Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void Gather(
    const T* sendbuf, T* recvbuf, size_t count, int root,
    comm_type& comm, gather_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_gather(sendbuf, recvbuf, count, root, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferGather: Invalid algorithm");
    }
  }

  template <typename T>
  static void Gather(
    T* buffer, size_t count, int root, comm_type& comm, gather_algo_type algo) {
    Gather(buffer, buffer, count, root, comm, algo);
  }

  template <typename T>
  static void NonblockingGather(
    const T* sendbuf, T* recvbuf, size_t count, int root,
    comm_type& comm, req_type& req, gather_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_gather(sendbuf, recvbuf, count, root, comm, internal_stream);
      break;
    default:
      throw_al_exception("Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingGather(
    T* buffer, size_t count, int root,
    comm_type& comm, req_type& req, gather_algo_type algo) {
    NonblockingGather<T>(buffer, buffer, count, root, comm, req, algo);
  }

  template <typename T>
  static void Reduce(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op, int root,
    comm_type& comm, reduce_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_reduce(sendbuf, recvbuf, count, op, root, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferReduce: Invalid algorithm");
    }
  }

  template <typename T>
  static void Reduce(
    T* buffer, size_t count, ReductionOperator op, int root, comm_type& comm,
    reduce_algo_type algo) {
    Reduce(buffer, buffer, count, op, root, comm, algo);
  }

  template <typename T>
  static void NonblockingReduce(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op, int root,
    comm_type& comm, req_type& req, reduce_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_reduce(sendbuf, recvbuf, count, op, root, comm, internal_stream);
      break;
    default:
      throw_al_exception("Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingReduce(
    T* buffer, size_t count, ReductionOperator op, int root,
    comm_type& comm, req_type& req, reduce_algo_type algo) {
    NonblockingReduce<T>(buffer, buffer, count, op, root, comm, req, algo);
  }

  template <typename T>
  static void Reduce_scatter(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op,
    comm_type& comm, reduce_scatter_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_reduce_scatter(sendbuf, recvbuf, count, op, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferReduce_scatter: Invalid algorithm");
    }
  }

  template <typename T>
  static void Reduce_scatter(
    T* buffer, size_t count, ReductionOperator op,
    comm_type& comm, reduce_scatter_algo_type algo) {
    Reduce_scatter(buffer, buffer, count, op, comm, algo);
  }

  template <typename T>
  static void NonblockingReduce_scatter(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op,
    comm_type& comm, req_type& req, reduce_scatter_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_reduce_scatter(sendbuf, recvbuf, count, op, comm, internal_stream);
      break;
    default:
      throw_al_exception("HostTransferReduce_scatter: Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingReduce_scatter(
    T* buffer, size_t count, ReductionOperator op,
    comm_type& comm, req_type& req, reduce_scatter_algo_type algo) {
    NonblockingReduce_scatter(buffer, buffer, count, op, comm, req, algo);
  }

  template <typename T>
  static void Scatter(
    const T* sendbuf, T* recvbuf, size_t count, int root,
    comm_type& comm, scatter_algo_type algo) {
    if (count == 0) return;
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_scatter(sendbuf, recvbuf, count, root, comm, comm.get_stream());
      break;
    default:
      throw_al_exception("HostTransferScatter: Invalid algorithm");
    }
  }

  template <typename T>
  static void Scatter(
    T* buffer, size_t count, int root, comm_type& comm, scatter_algo_type algo) {
    Scatter(buffer, buffer, count, root, comm, algo);
  }

  template <typename T>
  static void NonblockingScatter(
    const T* sendbuf, T* recvbuf, size_t count, int root,
    comm_type& comm, req_type& req, scatter_algo_type algo) {
    if (count == 0) return;
    cudaStream_t internal_stream = internal::cuda::get_internal_stream();
    sync_internal_stream_with_comm(internal_stream, comm);
    switch (algo) {
    case HostTransferCollectiveAlgorithm::automatic:
      do_scatter(sendbuf, recvbuf, count, root, comm, internal_stream);
      break;
    default:
      throw_al_exception("Invalid algorithm");
    }
    setup_completion_event(internal_stream, comm, req);
  }

  template <typename T>
  static void NonblockingScatter(
    T* buffer, size_t count, int root,
    comm_type& comm, req_type& req, scatter_algo_type algo) {
    NonblockingScatter<T>(buffer, buffer, count, root, comm, req, algo);
  }

  static std::string Name() { return "HostTransferBackend"; }

 private:
  /** Event for synchronizing between streams. */
  static cudaEvent_t sync_event;
  /**
   * Set up stream synchronization.
   * This will cause the provided internal stream to synchronize with the stream
   * associated with comm.
   */
  static void sync_internal_stream_with_comm(
    cudaStream_t internal_stream, comm_type& comm) {
    // We can reuse a single event for cudaStreamWaitEvent because it uses the
    // stream/event state when it is called.
    AL_CHECK_CUDA(cudaEventRecord(sync_event, comm.get_stream()));
    AL_CHECK_CUDA(cudaStreamWaitEvent(internal_stream, sync_event, 0));
  }
  /**
   * Set up the request for completion checking.
   * This uses an event recorded on the provided internal stream.
   */
  static void setup_completion_event(
    cudaStream_t internal_stream, comm_type& comm, req_type& req) {
    cudaEvent_t event = internal::cuda::get_cuda_event();
    AL_CHECK_CUDA(cudaEventRecord(event, internal_stream));
    req = std::make_shared<internal::ht::HostTransferRequest>(event, comm.get_stream());
  }

  /** Run a host-transfer allreduce. */
  template <typename T>
  static void do_allreduce(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op,
    comm_type& comm, cudaStream_t stream) {
    internal::ht::AllreduceAlState<T>* state = new internal::ht::AllreduceAlState<T>(
      sendbuf, recvbuf, count, op, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_send(
    const T* sendbuf, size_t count, int dest, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::SendAlState<T>* state =
      new internal::ht::SendAlState<T>(
        sendbuf, count, dest, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_recv(
    T* recvbuf, size_t count, int src, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::RecvAlState<T>* state =
      new internal::ht::RecvAlState<T>(
        recvbuf, count, src, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_sendrecv(
    const T* sendbuf, size_t send_count, int dest,
    T* recvbuf, size_t recv_count, int src, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::SendRecvAlState<T>* state =
      new internal::ht::SendRecvAlState<T>(
        sendbuf, send_count, dest, recvbuf, recv_count, src, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_allgather(
    const T* sendbuf, T* recvbuf, size_t count, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::AllgatherAlState<T>* state =
      new internal::ht::AllgatherAlState<T>(
        sendbuf, recvbuf, count, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_alltoall(
    const T* sendbuf, T* recvbuf, size_t count, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::AlltoallAlState<T>* state =
      new internal::ht::AlltoallAlState<T>(
        sendbuf, recvbuf, count, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_bcast(
    T* buf, size_t count, int root, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::BcastAlState<T>* state =
      new internal::ht::BcastAlState<T>(
        buf, count, root, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_gather(
    const T* sendbuf, T* recvbuf, size_t count, int root, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::GatherAlState<T>* state =
      new internal::ht::GatherAlState<T>(
        sendbuf, recvbuf, count, root, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_reduce(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op,
    int root, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::ReduceAlState<T>* state =
      new internal::ht::ReduceAlState<T>(
        sendbuf, recvbuf, count, op, root, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_reduce_scatter(
    const T* sendbuf, T* recvbuf, size_t count, ReductionOperator op,
    comm_type& comm, cudaStream_t stream) {
    internal::ht::ReduceScatterAlState<T>* state =
      new internal::ht::ReduceScatterAlState<T>(
        sendbuf, recvbuf, count, op, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }

  template <typename T>
  static void do_scatter(
    const T* sendbuf, T* recvbuf, size_t count, int root, comm_type& comm,
    cudaStream_t stream) {
    internal::ht::ScatterAlState<T>* state =
      new internal::ht::ScatterAlState<T>(
        sendbuf, recvbuf, count, root, comm, stream);
    internal::get_progress_engine()->enqueue(state);
  }
};

template <>
inline bool Test<HostTransferBackend>(typename HostTransferBackend::req_type& req) {
  if (req == HostTransferBackend::null_req) {
    return true;
  }
  // This is purely a host operation.
  bool r = cudaEventQuery(req->op_event) == cudaSuccess;
  if (r) {
    req = HostTransferBackend::null_req;
  }
  return r;
}

template <>
inline void Wait<HostTransferBackend>(typename HostTransferBackend::req_type& req) {
  if (req == HostTransferBackend::null_req) {
    return;
  }
  // Synchronize the original stream with the request.
  // This will not block the host.
  AL_CHECK_CUDA(cudaStreamWaitEvent(req->orig_stream, req->op_event, 0));
}

}  // namespace Al
