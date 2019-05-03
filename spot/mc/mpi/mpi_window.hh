#pragma once

#include <vector>

#include <mpi.h>

namespace spot
{
  class mpi_window
  {
    int *data_;
    MPI_Win win_;

  public:
    void init(size_t size, int default_value)
    {
      MPI_Alloc_mem(size * sizeof (int), MPI_INFO_NULL, &data_);
      memset(data_, default_value, size * sizeof (int));
      MPI_Win_create(data_, size * sizeof (int), sizeof (int), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &win_);
    }

    mpi_window() = default;

    int get(int rank, size_t index)
    {
      int value;
      MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win_);
      MPI_Get(&value, 1, MPI_INT, rank, index, 1, MPI_INT, win_);
      MPI_Win_unlock(rank, win_);
      return value;
    }

    void get(int rank, size_t index, std::vector<int>& values)
    {
      MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win_);
      MPI_Get(values.data(), values.size(), MPI_INT, rank, index, values.size(),
              MPI_INT, win_);
      MPI_Win_unlock(rank, win_);
    }

    void put(int rank, size_t index, int value)
    {
      MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win_);
      MPI_Put(&value, 1, MPI_INT, rank, index, 1, MPI_INT, win_);
      MPI_Win_unlock(rank, win_);
    }

    void put(int rank, size_t index, std::vector<int>& values, size_t size)
    {
      assert(values.size() == size);
      while (values.size() < size)
        values.push_back(0);
      MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win_);
      MPI_Put(values.data(), size, MPI_INT, rank, index, size, MPI_INT, win_);
      MPI_Win_unlock(rank, win_);
    }
  };
}
