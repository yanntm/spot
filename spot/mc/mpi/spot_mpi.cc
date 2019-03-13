#include "spot_mpi.hh"

#include <mpi.h>

namespace spot
{
  SpotMPI::SpotMPI()
   {
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  }

  SpotMPI::SpotMPI(std::string program, int procs)
  {
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    SPOT_ASSERT(world_size == 1);
    world_rank = 0;
    MPI_Comm everyone;
    MPI_Comm_spawn(program, MPI_ARGV_NULL, procs, MPI_INFO_NULL, world_rank,
                   MPI_COMM_SELF, &everyone, MPI_ERRCODES_IGNORE);
  }

  SpotMPI::~SpotMPI()
  {
    MPI_Finalize();
  }

  void SpotMPI::abort(int status)
  {
      MPI_Abort(MPI_COMM_WORLD, status);
  }
}
