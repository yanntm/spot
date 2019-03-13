#pragma once

#include <string>

namespace spot
{
  class SpotMPI
  {
    int world_size, world_rank;

    public:
      SpotMPI();
      SpotMPI(std::string program, int procs);
      ~SpotMPI();

      void abort(int status);
  };
}
