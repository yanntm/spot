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
    void init(size_t size)
    {
      MPI_Alloc_mem(size * sizeof (int), MPI_INFO_NULL, &data_);
      memset(data_, 0, size * sizeof (int));
      MPI_Win_create(data_, size * sizeof (int), sizeof (int), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &win_);
    }

    mpi_window() = default;

    ~mpi_window()
    {
      MPI_Free_mem(data_);
    }

    int get(int rank, size_t index)
    {
      return get(rank, index, 1)[0];
    }

    std::vector<int> get(int rank, size_t index, size_t size)
    {
      MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win_);
      std::vector<int> values(size);
      MPI_Get(values.data(), size, MPI_INT, rank, index, size, MPI_INT, win_);
      MPI_Win_unlock(rank, win_);
      return values;
    }

    void put(int rank, size_t index, int value)
    {
      put(rank, index, std::vector<int>(1, value), 1);
    }

    void put(int rank, size_t index, std::vector<int> values, size_t size)
    {
      assert(values.size() <= size);
      while (values.size() < size)
        values.push_back(0);
      MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win_);
      MPI_Put(values.data(), size, MPI_INT, rank, index, size, MPI_INT, win_);
      MPI_Win_unlock(rank, win_);
    }
  };
}
