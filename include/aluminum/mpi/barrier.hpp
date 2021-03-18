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

#include "aluminum/progress.hpp"
#include "aluminum/mpi/base_state.hpp"
#include "aluminum/mpi/communicator.hpp"
#include "aluminum/mpi/utils.hpp"

namespace Al {
namespace internal {
namespace mpi {

inline void passthrough_barrier(MPICommunicator& comm) {
  MPI_Barrier(comm.get_comm());
}

class BarrierAlState : public MPIState {
public:
  BarrierAlState(MPICommunicator& comm_, AlRequest req_)
      : MPIState(req_), comm(comm_.get_comm()) {}
  ~BarrierAlState() override {}

  std::string get_name() const override { return "MPIBarrier"; }

protected:
  void start_mpi_op() override {
    MPI_Ibarrier(comm, get_mpi_req());
  }

private:
  MPI_Comm comm;
};

inline void passthrough_nb_barrier(MPICommunicator& comm, AlRequest& req) {
  req = internal::get_free_request();
  internal::mpi::BarrierAlState* state =
    new internal::mpi::BarrierAlState(comm, req);
  get_progress_engine()->enqueue(state);
}

} // namespace mpi
} // namespace internal
} // namespace Al