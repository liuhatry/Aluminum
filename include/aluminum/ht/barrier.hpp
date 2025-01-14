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

#include "aluminum/cuda/cuda.hpp"
#include "aluminum/ht/communicator.hpp"
#include "aluminum/ht/base_state.hpp"

namespace Al {
namespace internal {
namespace ht {

class BarrierAlState : public HostTransferCollectiveSignalAtEndState {
public:
  BarrierAlState(HostTransferCommunicator& comm_, cudaStream_t stream_) :
    HostTransferCollectiveSignalAtEndState(stream_),
    comm(comm_.get_comm()) {
    // Just wait until we should start this.
    start_event.record(stream_);

    // Have the device wait on the host.
    gpu_wait.wait(stream_);
  }

  std::string get_name() const override { return "HTBarrier"; }

protected:
  void start_mpi_op() override {
    MPI_Ibarrier(comm, get_mpi_req());
  }

private:
  MPI_Comm comm;
};

}  // namespace ht
}  // namespace internal
}  // namespace Al
