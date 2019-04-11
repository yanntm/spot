#pragma once

#include <string>

#include <mpi.h>

namespace spot
{
  class SpotMPI
  {
    public:
      int world_size, world_rank;

      SpotMPI()
      {
        MPI_Init(NULL, NULL);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
      }

      SpotMPI(std::string program, int procs)
      {
        MPI_Init(NULL, NULL);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        SPOT_ASSERT(world_size == 1);
        world_rank = 0;
        MPI_Comm everyone;
        MPI_Comm_spawn(program.c_str(), MPI_ARGV_NULL, procs, MPI_INFO_NULL,
                       world_rank, MPI_COMM_SELF, &everyone,
                       MPI_ERRCODES_IGNORE);
      }

      ~SpotMPI()
      {
        MPI_Finalize();
      }

      void abort(int status)
      {
        MPI_Abort(MPI_COMM_WORLD, status);
      }
  };
}
